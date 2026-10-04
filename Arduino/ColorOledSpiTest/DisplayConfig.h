#pragma once

#include <Arduino.h>

namespace DisplayConfig {
constexpr int CLOCK_PIN = 18;
constexpr int DATA_PIN = 19; // SPI MOSI, despite the module's SDA label.
constexpr int RESET_PIN = 16; // DevKit label RX2.
constexpr int DATA_COMMAND_PIN = 17; // DevKit label TX2.
constexpr int CHIP_SELECT_PIN = 26;
constexpr int PCA_OUTPUT_ENABLE_PIN = 25;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SPI_FREQUENCY_HZ = 1000000;
constexpr uint32_t PAGE_INTERVAL_MS = 2000;
} // namespace DisplayConfig
