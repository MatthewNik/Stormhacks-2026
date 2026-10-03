#include "SensorConfig.h"

uint32_t lastReportMs = 0;

void printSensorReadings() {
  for (const auto &sensor : SensorConfig::SENSORS) {
    if (!sensor.enabled) continue;
    const bool pinIsHigh = digitalRead(sensor.gpio) == HIGH;
    Serial.print(pinIsHigh ? "HIGH-" : "LOW-");
    Serial.println(sensor.sensorNumber);
  }
}

void setup() {
  // Keep a connected PCA9685 disabled while this sensor-only sketch is running.
  digitalWrite(SensorConfig::PCA_OUTPUT_ENABLE_PIN, HIGH);
  pinMode(SensorConfig::PCA_OUTPUT_ENABLE_PIN, OUTPUT);
  Serial.begin(SensorConfig::SERIAL_BAUD);

  for (const auto &sensor : SensorConfig::SENSORS) {
    if (sensor.enabled) pinMode(sensor.gpio, INPUT);
  }
  delay(SensorConfig::STARTUP_SETTLE_MS);

  printSensorReadings();
  lastReportMs = millis();
}

void loop() {
  const uint32_t nowMs = millis();
  // Unsigned elapsed time remains valid across millis() rollover.
  if (uint32_t(nowMs - lastReportMs) >= SensorConfig::REPORT_INTERVAL_MS) {
    lastReportMs = nowMs;
    printSensorReadings();
  }
  delay(1);
}
