#pragma once
#include <stdint.h>
#include <stddef.h>
struct TwoWire {
  uint8_t registers[256] = {};
  uint8_t selected = 0;
  uint8_t pendingRegister = 0;
  bool beginOK = true, ack = true, shortRead = false;
  bool buffered = false, nonStop = false, rejectRepeatedStart = false, pointerAck = true, emptyRead = false;
  int sda = -1, scl = -1, timeout = 0;
  uint32_t frequency = 0;
  unsigned stoppedSelections = 0, repeatedReads = 0;
  bool begin(int data, int clock, uint32_t hz = 0) { sda = data; scl = clock; frequency = hz; return beginOK; }
  void setTimeOut(int value) { timeout = value; }
  void beginTransmission(uint8_t) { buffered = nonStop = false; }
  size_t write(uint8_t value) { pendingRegister = value; buffered = true; return 1; }
  int endTransmission(bool stop = true) {
    nonStop = !stop;
    if (!stop) return 0; // ESP32 defers this transaction until requestFrom.
    if (!ack) return 4;
    if (buffered) {
      if (!pointerAck) return 2;
      selected = pendingRegister; buffered = false; ++stoppedSelections;
    }
    return 0;
  }
  int requestFrom(uint8_t, size_t, bool = true) {
    if (nonStop) {
      nonStop = false; ++repeatedReads;
      if (rejectRepeatedStart || !pointerAck) return 0;
      selected = pendingRegister; buffered = false;
    }
    return !ack || shortRead ? 0 : 1;
  }
  int read() { return emptyRead ? -1 : registers[selected]; }
};
inline TwoWire Wire;
