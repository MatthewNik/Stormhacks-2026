#pragma once
#include <stdint.h>
struct TwoWire {
  uint8_t registers[256] = {};
  uint8_t selected = 0;
  bool beginOK = true, ack = true, shortRead = false;
  int sda = -1, scl = -1, timeout = 0;
  bool begin(int data, int clock) { sda = data; scl = clock; return beginOK; }
  void setTimeOut(int value) { timeout = value; }
  void beginTransmission(uint8_t) {}
  void write(uint8_t value) { selected = value; }
  int endTransmission(bool = true) { return ack ? 0 : 4; }
  int requestFrom(uint8_t, uint8_t) { return shortRead ? 0 : 1; }
  int read() { return registers[selected]; }
};
inline TwoWire Wire;
