#pragma once
#include "Wire.h"
#include <vector>
struct PwmWrite { uint8_t channel; uint16_t on, off; };
inline std::vector<PwmWrite> pwmWrites;
inline bool driverBeginOK = true, pwmWriteOK = true, frequencyOK = true;
inline int pwmFailNext = 0; // fail this many upcoming writes, then succeed
inline uint32_t oscillator = 0;
inline float configuredFrequency = 0;
struct Adafruit_PWMServoDriver {
  Adafruit_PWMServoDriver(uint8_t, TwoWire &) {}
  bool begin() { return driverBeginOK; }
  void setOscillatorFrequency(uint32_t hz) { oscillator = hz; }
  void setPWMFreq(float hz) { configuredFrequency = hz; Wire.registers[0xfe] = frequencyOK ? 121 : 0; }
  uint8_t setPWM(uint8_t ch, uint16_t on, uint16_t off) {
    if (!pwmWriteOK) return 4;
    if (pwmFailNext > 0) { --pwmFailNext; return 4; }
    pwmWrites.push_back({ch,on,off}); return 0;
  }
};
