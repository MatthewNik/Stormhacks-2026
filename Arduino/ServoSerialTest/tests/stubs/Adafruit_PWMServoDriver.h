#pragma once
#include "Wire.h"
struct Adafruit_PWMServoDriver {
  bool failWrites = false;
  std::vector<std::tuple<int, int, int>> writes;
  Adafruit_PWMServoDriver(uint8_t, WireStub &) {}
  bool begin() { return Wire.responds; }
  void setOscillatorFrequency(uint32_t) {}
  void setPWMFreq(float) {}
  uint8_t setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    if (failWrites || !Wire.responds) return 1;
    writes.emplace_back(channel, on, off);
    return 0;
  }
};
