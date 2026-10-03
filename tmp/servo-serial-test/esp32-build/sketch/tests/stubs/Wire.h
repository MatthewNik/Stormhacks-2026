#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ServoSerialTest\\tests\\stubs\\Wire.h"
#pragma once
#include "Arduino.h"
struct WireStub {
  bool responds = true;
  bool begin(int, int) { return responds; }
  void setTimeOut(int) {}
  void beginTransmission(uint8_t) {}
  void write(uint8_t) {}
  int endTransmission(bool = true) { return responds ? 0 : 4; }
  int requestFrom(uint8_t, uint8_t) { return responds ? 1 : 0; }
  int read() { return 121; }
};
inline WireStub Wire;
