#include <Servo.h>
#include <stdlib.h>
#include <string.h>

// Match the project's nominal PCA pulse mapping for transferable calibration.
const byte SERVO_PIN = 9;
const int MIN_PULSE_US = 500;
const int MAX_PULSE_US = 2500;

Servo motor;
char line[32];
byte used = 0;
bool overflow = false;
int lastAngle = -1;

void help() {
  Serial.println(F("Single-servo calibration: D9, 115200 baud, Newline."));
  Serial.println(F("Enter 0..180 (e.g. 90), + or - for 1 degree, off, status, help."));
  Serial.println(F("Start unloaded at 90. Approach endpoints slowly; stop if binding."));
  Serial.println(F("No position feedback. off removes pulses/holding torque, not power."));
}

void status() {
  Serial.print(motor.attached() ? F("ON") : F("OFF"));
  if (lastAngle >= 0) {
    Serial.print(F(" | last commanded angle="));
    Serial.print(lastAngle);
    Serial.print(F(" | pulse_us="));
    Serial.print(map(lastAngle, 0, 180, MIN_PULSE_US, MAX_PULSE_US));
  }
  Serial.println();
}

void setAngle(int angle) {
  if (angle < 0 || angle > 180) {
    Serial.println(F("ERROR: angle must be 0..180."));
    return;
  }
  int pulse = map(angle, 0, 180, MIN_PULSE_US, MAX_PULSE_US);
  // Preload the requested pulse so reattaching does not first command neutral.
  motor.writeMicroseconds(pulse);
  if (!motor.attached()) motor.attach(SERVO_PIN, MIN_PULSE_US, MAX_PULSE_US);
  lastAngle = angle;
  status();
}

void command(char *text) {
  while (*text == ' ' || *text == '\t') ++text;
  size_t length = strlen(text);
  while (length && (text[length - 1] == ' ' || text[length - 1] == '\t'))
    text[--length] = '\0';
  if (!length) return;
  if (!strcmp(text, "off")) {
    motor.detach();
    digitalWrite(SERVO_PIN, LOW);
    status();
  } else if (!strcmp(text, "help")) {
    help();
  } else if (!strcmp(text, "status")) {
    status();
  } else if (!strcmp(text, "+") || !strcmp(text, "-")) {
    if (lastAngle < 0 || !motor.attached()) {
      Serial.println(F("ERROR: enter an angle first."));
      return;
    }
    setAngle(lastAngle + (text[0] == '+' ? 1 : -1));
  } else {
    // Reject signs, decimals, trailing text, and numbers too long to be angles.
    if (length > 3) {
      Serial.println(F("ERROR: use 0..180, +, -, off, status, help."));
      return;
    }
    for (size_t i = 0; i < length; ++i) {
      if (text[i] < '0' || text[i] > '9') {
        Serial.println(F("ERROR: use an integer angle 0..180."));
        return;
      }
    }
    setAngle(atoi(text));
  }
}

void setup() {
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  Serial.begin(115200);
  help();
  Serial.println(F("Startup: OFF. Enter an angle to enable pulses."));
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (overflow) Serial.println(F("ERROR: line too long; command discarded."));
      else {
        line[used] = '\0';
        command(line);
      }
      used = 0;
      overflow = false;
    } else if (!overflow) {
      if (used < sizeof(line) - 1) line[used++] = c;
      else overflow = true;
    }
  }
}
