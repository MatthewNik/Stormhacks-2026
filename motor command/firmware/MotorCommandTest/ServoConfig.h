#pragma once

#include <Arduino.h>

namespace ServoConfig {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr int OUTPUT_ENABLE_PIN = 25;
constexpr uint8_t PCA_ADDRESS = 0x40;
constexpr uint8_t SERVO_COUNT = 8;
constexpr uint8_t PCA_CHANNEL_COUNT = 16;
constexpr float PWM_FREQUENCY_HZ = 50.0f;
constexpr uint32_t OSCILLATOR_FREQUENCY_HZ = 25000000;
constexpr size_t COMMAND_BUFFER_SIZE = 96;
constexpr uint32_t BUS_CHECK_INTERVAL_MS = 1000;
constexpr uint32_t LINK_TIMEOUT_MS = 2000;

struct PulseEndpoints {
  uint16_t minimumUs;
  uint16_t maximumUs;
};

// Nominal package endpoints; reduce individual ranges if a servo hits a stop.
constexpr PulseEndpoints ENDPOINTS[SERVO_COUNT] = {
    {500, 2500}, // Channel 0
    {500, 2500}, // Channel 1
    {500, 2500}, // Channel 2
    {500, 2500}, // Channel 3
    {500, 2500}, // Channel 4
    {500, 2500}, // Channel 5
    {500, 2500}, // Channel 6
    {500, 2500}  // Channel 7
};
} // namespace ServoConfig
