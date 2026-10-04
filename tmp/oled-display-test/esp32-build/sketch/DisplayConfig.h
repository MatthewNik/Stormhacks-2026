#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\OledDisplayTest\\DisplayConfig.h"
#pragma once

#include <Arduino.h>

namespace DisplayConfig {
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr int PCA_OUTPUT_ENABLE_PIN = 25;
constexpr int RESET_PIN = -1; // Four-wire module has no separate reset connection.
constexpr uint16_t WIDTH = 128;
constexpr uint16_t HEIGHT = 64;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t I2C_FREQUENCY_HZ = 100000;
constexpr uint32_t FRAME_INTERVAL_MS = 250;
constexpr uint32_t PAGE_INTERVAL_MS = 3000;
// Zero automatically selects the sole responding address among 0x3C and 0x3D.
constexpr uint8_t OLED_ADDRESS = 0;
} // namespace DisplayConfig
