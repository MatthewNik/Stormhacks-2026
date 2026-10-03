#pragma once

#include <Arduino.h>

namespace SensorConfig {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t STARTUP_SETTLE_MS = 50;
constexpr uint32_t REPORT_INTERVAL_MS = 50;
constexpr int PCA_OUTPUT_ENABLE_PIN = 25;

struct SensorSettings {
  uint8_t gpio;
  uint8_t sensorNumber;
  bool enabled;
};

// Report raw pin levels: active-low modules read LOW when detecting.
// Disable entries whose sensors are not connected to avoid floating-input messages.
constexpr SensorSettings SENSORS[] = {
    {34, 1, true},
    {35, 2, true},
    {36, 3, true}, // DevKit label VP
    {39, 4, true}, // DevKit label VN
    {32, 5, true},
    {33, 6, true},
    {27, 7, true}
};
constexpr size_t SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);
} // namespace SensorConfig
