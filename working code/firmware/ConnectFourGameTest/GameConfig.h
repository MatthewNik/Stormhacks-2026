#pragma once
#include <stdint.h>

#ifndef MENU_TEST_ONLY
#define MENU_TEST_ONLY 0
#endif
#ifndef ROBOT_IR_CONFIRM_BUILD
#define ROBOT_IR_CONFIRM_BUILD 0
#endif
namespace Config {
constexpr bool MENU_ONLY = MENU_TEST_ONLY != 0;
constexpr int SDA = 21, SCL = 22, OE = 25;
constexpr uint8_t PCA_ADDRESS = 0x40;
constexpr uint32_t I2C_HZ = 100000;
constexpr int OLED_CLOCK = 18, OLED_DATA = 19, OLED_RESET = 16, OLED_DC = 17, OLED_CS = 26;
constexpr uint32_t SERIAL_BAUD = 115200, SPI_HZ = 1000000;
constexpr int BUTTON_LEFT = 13, BUTTON_RIGHT = 14, BUTTON_CENTRE = 23;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t OSCILLATOR_HZ = 25000000;
constexpr float PWM_HZ = 50;
// Stagger flap command starts so no more than about two servos travel at once;
// six hatches closing together can disturb the servo supply, ground and PCA bus.
// Motor 7 has separate feed sequencing.
constexpr uint32_t COMMAND_GAP_MS = 150, SETTLE_MS = 300;
// Bounded I2C attempts per PCA register access; a single corrupted transaction
// during servo current spikes must not latch a fault on its own.
constexpr unsigned PCA_IO_ATTEMPTS = 3;
// Calibrated servos hold their commanded positions until stop/fault disables OE.
constexpr uint32_t BUS_CHECK_MS = 1000;
// Column 1/PCA0 is front-right: IR DO34; column 7/PCA6 is front-left: DO27.
constexpr int SENSOR_PINS[7] = {34,35,36,39,32,33,27};
constexpr bool SENSOR_ACTIVE_LOW[7] = {true,true,true,true,true,true,true};
constexpr uint32_t DETECT_US = 2000, CLEAR_US = 20000, STABLE_CLEAR_US = 100000;
// Commit an IR level only after it has been unchanged this long. Equal to the
// minimum detection, so comparator chatter cannot fill the edge queue.
constexpr uint32_t SENSOR_DEBOUNCE_US = DETECT_US;
// IR sampling period; eight samples fit inside one debounce interval.
constexpr uint32_t SENSOR_SAMPLE_US = 250;
// A beam blocked this long is the only human-turn sensor condition that pauses.
constexpr uint32_t STUCK_US = 4000000, BASELINE_TIMEOUT_MS = 2000, PASSAGE_TIMEOUT_MS = 3000;
constexpr uint32_t INDEXER_SETTLE_MS = 300, INDEXER_LOAD_MS = 500;
// false: the robot turn is timed only (open target, release, return indexer to
// load, close, commit) and IR is ignored, so it never pauses for a missing or
// noisy passage. The board then assumes the disc landed in the chosen column.
constexpr bool ROBOT_IR_CONFIRM = ROBOT_IR_CONFIRM_BUILD != 0;
// Time after the 180 release before the indexer returns to load and doors close.
constexpr uint32_t ROBOT_DROP_MS = 1000;
constexpr unsigned EDGE_QUEUE_SIZE = 64;
constexpr int SEARCH_DEPTHS[] = {2, 4, 5};
// All seven hatches enabled after individual unloaded position validation.
constexpr bool HATCH_ENABLED[7] = {true, true, true, true, true, true, true};
struct HatchCalibration { uint16_t closed, open, minimumUs, maximumUs; };
constexpr HatchCalibration INDEXER = {180,110,500,2500}; // release, load, pulse endpoints
// PCA0 / column 1 is front-right; PCA6 / column 7 is front-left.
// Down is closed; up is open. Angles use the Uno 500..2500 us mapping.
constexpr HatchCalibration HATCHES[7] = {
  {15,115,500,2500}, {8,110,500,2500}, {11,100,500,2500}, {12,105,500,2500}, {11,110,500,2500}, {12,115,500,2500}, {12,110,500,2500}
};
}
