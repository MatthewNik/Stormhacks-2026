#pragma once
#include <stdint.h>

namespace Config {
constexpr int SDA = 21, SCL = 22, OE = 25;
constexpr uint8_t PCA_ADDRESS = 0x40;
constexpr int OLED_CLOCK = 18, OLED_DATA = 19, OLED_RESET = 16, OLED_DC = 17, OLED_CS = 26;
constexpr uint32_t SERIAL_BAUD = 115200, SPI_HZ = 1000000;
constexpr uint32_t OSCILLATOR_HZ = 25000000;
constexpr float PWM_HZ = 50;
constexpr uint32_t COMMAND_GAP_MS = 50, SETTLE_MS = 300;
// Unloaded bench mode: release pulses after travel; do not hold unknown servos indefinitely.
constexpr uint32_t SERVO_DRIVE_MS = 300;
constexpr uint32_t ROBOT_DRIVE_MS = 1600;
static_assert(SERVO_DRIVE_MS > 0 && ROBOT_DRIVE_MS <= 2000, "Keep bench servo drive bounded");
constexpr uint32_t BUS_CHECK_MS = 1000;
constexpr int SENSOR_PINS[7] = {34,35,36,39,32,33,27};
constexpr bool SENSOR_ACTIVE_LOW[7] = {true,true,true,true,true,true,true};
constexpr uint32_t DETECT_US = 2000, CLEAR_US = 20000, STABLE_CLEAR_US = 100000;
constexpr uint32_t STUCK_US = 1000000, BASELINE_TIMEOUT_MS = 2000, PASSAGE_TIMEOUT_MS = 3000;
constexpr uint32_t INDEXER_SETTLE_MS = 300, INDEXER_LOAD_MS = 500;
constexpr unsigned EDGE_QUEUE_SIZE = 64;
constexpr int SEARCH_DEPTHS[] = {2, 4, 5};
// Enable isolated channels only after unloaded position/feedback validation.
constexpr bool HATCH_ENABLED[7] = {true, true, false, false, false, true, true};
struct HatchCalibration { uint16_t closed, open, minimumUs, maximumUs; };
constexpr HatchCalibration INDEXER = {0,90,500,2500}; // release, load, pulse endpoints
// Entry index is the PCA channel (column minus one). Calibrate each unloaded servo.
constexpr HatchCalibration HATCHES[7] = {
  {0,90,500,2500}, {0,90,500,2500}, {0,90,500,2500}, {0,90,500,2500},
  {0,90,500,2500}, {0,90,500,2500}, {0,90,500,2500}
};
}
