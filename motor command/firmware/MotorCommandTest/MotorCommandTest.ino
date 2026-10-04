#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <stdlib.h>
#include <string.h>
#include "ServoConfig.h"

Adafruit_PWMServoDriver servoDriver(ServoConfig::PCA_ADDRESS, Wire);
char commandBuffer[ServoConfig::COMMAND_BUFFER_SIZE];
size_t commandLength = 0;
bool discardingOversizedLine = false;
bool invalidLine = false;
bool controllerReady = false;
uint8_t pwmPrescale = 0;
uint32_t lastBusCheckMs = 0, lastContactMs = 0;
bool activeChannels[ServoConfig::SERVO_COUNT] = {};
int commandedAngles[ServoConfig::SERVO_COUNT] = {-1,-1,-1,-1,-1,-1,-1,-1};
void updateEnable() {
  bool any = false; for (bool active : activeChannels) any |= active;
  digitalWrite(ServoConfig::OUTPUT_ENABLE_PIN, any && controllerReady ? LOW : HIGH);
}

void disableOutputs() {
  digitalWrite(ServoConfig::OUTPUT_ENABLE_PIN, HIGH);
}

void latchControllerFault() {
  disableOutputs();
  for (bool &active : activeChannels) active = false;
  controllerReady = false;
  Serial.println("ERROR: PCA communication/configuration fault. Outputs disabled; reset ESP32 after repair.");
}

bool pcaResponds() {
  Wire.beginTransmission(ServoConfig::PCA_ADDRESS);
  return Wire.endTransmission() == 0;
}

bool readPwmPrescale() {
  Wire.beginTransmission(ServoConfig::PCA_ADDRESS);
  Wire.write(uint8_t(0xFE)); // PCA9685 PRE_SCALE register.
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ServoConfig::PCA_ADDRESS, uint8_t(1)) != 1) return false;
  pwmPrescale = uint8_t(Wire.read());
  const float actualFrequency = float(ServoConfig::OSCILLATOR_FREQUENCY_HZ) /
                                (4096.0f * (pwmPrescale + 1));
  return actualFrequency >= 45.0f && actualFrequency <= 55.0f;
}

bool stopChannel(uint8_t channel) {
  if (servoDriver.setPWM(channel, 0, 4096) != 0) {
    latchControllerFault();
    return false;
  }
  if (channel < ServoConfig::SERVO_COUNT) activeChannels[channel] = false;
  updateEnable();
  return true;
}

bool stopAllChannels() {
  disableOutputs();
  for (bool &active : activeChannels) active = false;
  for (uint8_t channel = 0; channel < ServoConfig::PCA_CHANNEL_COUNT; ++channel) {
    if (!stopChannel(channel)) return false;
  }
  return true;
}

void printHelp() {
  Serial.println("servo <0-7> <0-180>  : command approximate angle, e.g. servo 1 50");
  Serial.println("off <0-7>           : stop pulses on one channel");
  Serial.println("off all             : disable all outputs");
  Serial.println("status              : show commanded positions and pulse state");
  Serial.println("help                : show commands");
  Serial.println("Pi terminal sends heartbeats; missing heartbeats stop pulses after 2 seconds.");
  Serial.println("Use 115200 baud and Newline, Carriage Return, or Both.");
}

bool parseIntegerInRange(const char *token, long maximum, long &value) {
  if (token == nullptr || *token == '\0') return false;
  value = 0;
  for (const char *cursor = token; *cursor; ++cursor) {
    if (*cursor < '0' || *cursor > '9') return false;
    // Bound before multiplication, including arbitrarily long numeric tokens.
    const int digit = *cursor - '0';
    if (value > (maximum - digit) / 10 || digit > maximum) return false;
    value = value * 10 + digit;
    if (value > maximum) return false;
  }
  return true;
}

bool commandServo(uint8_t channel, uint16_t degrees) {
  const auto &endpoints = ServoConfig::ENDPOINTS[channel];
  const uint32_t pulseUs = endpoints.minimumUs +
      (uint32_t(endpoints.maximumUs - endpoints.minimumUs) * degrees + 90) / 180;
  // Use the configured prescaler, rather than assuming an exact 20 ms period.
  const uint64_t numerator = uint64_t(pulseUs) * ServoConfig::OSCILLATOR_FREQUENCY_HZ;
  const uint64_t denominator = uint64_t(pwmPrescale + 1) * 1000000;
  const uint16_t pulseTicks = uint16_t((numerator + denominator / 2) / denominator);
  if (servoDriver.setPWM(channel, 0, pulseTicks) != 0) {
    latchControllerFault();
    return false;
  }
  activeChannels[channel] = true; commandedAngles[channel] = degrees;
  lastContactMs = millis(); updateEnable();
  Serial.print("OK: channel "); Serial.print(channel);
  Serial.print(" commanded to "); Serial.print(degrees);
  Serial.print(" degrees ("); Serial.print(pulseUs); Serial.println(" us nominal).");
  return true;
}

void handleCommand(char *line) {
  char *tokens[4] = {};
  char *savePointer = nullptr;
  size_t tokenCount = 0;
  for (char *token = strtok_r(line, " \t", &savePointer); token;
       token = strtok_r(nullptr, " \t", &savePointer)) {
    if (tokenCount == 4) break;
    tokens[tokenCount++] = token;
  }
  if (tokenCount == 0) return;
  if (strcmp(tokens[0], "help") == 0 && tokenCount == 1) {
    printHelp();
    return;
  }
  if (strcmp(tokens[0], "info") == 0 && tokenCount == 1) {
    Serial.println(controllerReady ? "MOTOR_TEST v1 READY" : "MOTOR_TEST v1 FAULT"); return;
  }
  if (strcmp(tokens[0], "ping") == 0 && tokenCount == 1) {
    if (controllerReady) lastContactMs = millis(); return;
  }
  if (strcmp(tokens[0], "status") == 0 && tokenCount == 1) {
    Serial.println(controllerReady ? "MOTOR_TEST v1 READY" : "MOTOR_TEST v1 FAULT");
    for (uint8_t c = 0; c < ServoConfig::SERVO_COUNT; ++c) {
      Serial.print("channel "); Serial.print(c); Serial.print(activeChannels[c] ? " ON angle=" : " OFF last_angle=");
      Serial.println(commandedAngles[c]);
    }
    return;
  }
  long channel = 0;
  long degrees = 0;
  const bool isServoCommand = strcmp(tokens[0], "servo") == 0 && tokenCount == 3 &&
      parseIntegerInRange(tokens[1], 7, channel) &&
      parseIntegerInRange(tokens[2], 180, degrees);
  const bool isOffAll = strcmp(tokens[0], "off") == 0 && tokenCount == 2 &&
      strcmp(tokens[1], "all") == 0;
  const bool isOffChannel = strcmp(tokens[0], "off") == 0 && tokenCount == 2 &&
      parseIntegerInRange(tokens[1], 7, channel);
  if (!isServoCommand && !isOffAll && !isOffChannel) {
    Serial.println("ERROR: invalid command. Use help; channels 0-7, integer degrees 0-180.");
    return;
  }
  if (!controllerReady) {
    Serial.println("ERROR: controller unavailable. Repair wiring and reset ESP32.");
    return;
  }
  if (isServoCommand) {
    commandServo(uint8_t(channel), uint16_t(degrees));
  } else if (isOffAll) {
    if (stopAllChannels()) Serial.println("OK: all outputs disabled.");
  } else {
    if (stopChannel(uint8_t(channel))) {
      Serial.print("OK: pulses stopped on channel "); Serial.println(channel);
    }
  }
}

void readSerialCommands() {
  int remaining = Serial.available(); if (remaining > 64) remaining = 64;
  while (remaining-- > 0) {
    const char character = Serial.read();
    if (character == '\r' || character == '\n') {
      if (discardingOversizedLine) {
        Serial.println("ERROR: command too long; entire line ignored.");
      } else if (invalidLine) {
        Serial.println("ERROR: invalid character; entire line ignored.");
      } else if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
      }
      commandLength = 0;
      discardingOversizedLine = false;
      invalidLine = false;
    } else if (!discardingOversizedLine) {
      if ((uint8_t(character) < 32 && character != '\t') || uint8_t(character) >= 127) {
        invalidLine = true;
      }
      if (commandLength < sizeof(commandBuffer) - 1) {
        commandBuffer[commandLength++] = character;
      } else {
        discardingOversizedLine = true;
      }
    }
  }
}

void setup() {
  // Preload HIGH before changing the pin to an output; external pull-up covers reset.
  digitalWrite(ServoConfig::OUTPUT_ENABLE_PIN, HIGH);
  pinMode(ServoConfig::OUTPUT_ENABLE_PIN, OUTPUT);
  Serial.begin(ServoConfig::SERIAL_BAUD);
  Serial.println("\nMotor command test. No automatic motion.");
  printHelp();
  for (const auto &endpoints : ServoConfig::ENDPOINTS) {
    if (endpoints.minimumUs < 500 || endpoints.maximumUs > 2500 ||
        endpoints.minimumUs >= endpoints.maximumUs) {
      latchControllerFault();
      return;
    }
  }
  if (!Wire.begin(ServoConfig::SDA_PIN, ServoConfig::SCL_PIN)) {
    latchControllerFault();
    return;
  }
  Wire.setTimeOut(50);
  if (!servoDriver.begin()) {
    latchControllerFault();
    return;
  }
  servoDriver.setOscillatorFrequency(ServoConfig::OSCILLATOR_FREQUENCY_HZ);
  servoDriver.setPWMFreq(ServoConfig::PWM_FREQUENCY_HZ);
  if (!readPwmPrescale() || !stopAllChannels()) {
    latchControllerFault();
    return;
  }
  controllerReady = true;
  lastBusCheckMs = lastContactMs = millis();
  Serial.println("READY: channels 0-7 available; outputs disabled until a servo command.");
}

void loop() {
  readSerialCommands();
  const uint32_t now = millis();
  bool any = false; for (bool active : activeChannels) any |= active;
  if (controllerReady && any && uint32_t(now-lastContactMs) >= ServoConfig::LINK_TIMEOUT_MS) {
    if (stopAllChannels()) Serial.println("STOP: Pi heartbeat expired; all outputs disabled.");
  }
  if (controllerReady && uint32_t(now - lastBusCheckMs) >= ServoConfig::BUS_CHECK_INTERVAL_MS) {
    lastBusCheckMs = now;
    const uint8_t expected = pwmPrescale;
    if (!pcaResponds() || !readPwmPrescale() || pwmPrescale != expected) latchControllerFault();
  }
}
