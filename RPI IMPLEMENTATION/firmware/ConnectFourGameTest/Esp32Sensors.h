#pragma once
#include <Arduino.h>
#include "Sensors.h"

class Esp32Sensors : public SensorPort {
  struct Context { Esp32Sensors *owner; uint8_t column; } contexts[7];
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  SensorEdge queue[Config::EDGE_QUEUE_SIZE];
  unsigned head = 0, tail = 0, count = 0;
  bool capture = false, overflow = false;
  bool level(uint8_t c) const {
    const bool high = digitalRead(Config::SENSOR_PINS[c]) == HIGH;
    return Config::SENSOR_ACTIVE_LOW[c] ? !high : high;
  }
  static void ARDUINO_ISR_ATTR interrupt(void *arg) {
    auto &ctx = *static_cast<Context *>(arg); auto &self = *ctx.owner;
    portENTER_CRITICAL_ISR(&self.mux);
    if (self.capture) {
      if (self.count == Config::EDGE_QUEUE_SIZE) self.overflow = true;
      else {
        self.queue[self.head] = {ctx.column,self.level(ctx.column),micros()};
        self.head = (self.head+1)%Config::EDGE_QUEUE_SIZE; ++self.count;
      }
    }
    portEXIT_CRITICAL_ISR(&self.mux);
  }
public:
  void begin() {
    for (uint8_t c = 0; c < 7; ++c) {
      pinMode(Config::SENSOR_PINS[c],INPUT);
      contexts[c] = {this,c};
      attachInterruptArg(Config::SENSOR_PINS[c],interrupt,&contexts[c],CHANGE);
    }
  }
  uint32_t reset(bool enabled, bool levels[7]) override {
    portENTER_CRITICAL(&mux);
    capture = false; head = tail = count = 0; overflow = false;
    const uint32_t stamp = micros();
    for (uint8_t c = 0; c < 7; ++c) levels[c] = enabled && level(c);
    capture = enabled;
    portEXIT_CRITICAL(&mux); return stamp;
  }
  bool pop(SensorEdge &edge) override {
    portENTER_CRITICAL(&mux);
    const bool found = count != 0;
    if (found) { edge = queue[tail]; tail = (tail+1)%Config::EDGE_QUEUE_SIZE; --count; }
    portEXIT_CRITICAL(&mux); return found;
  }
  bool overflowed() override {
    portENTER_CRITICAL(&mux); const bool value = overflow; portEXIT_CRITICAL(&mux); return value;
  }
  bool sealClear() override {
    portENTER_CRITICAL(&mux);
    bool okay = capture && !overflow && count == 0;
    for (uint8_t c = 0; c < 7 && okay; ++c) okay = !level(c);
    if (okay) capture = false;
    portEXIT_CRITICAL(&mux); return okay;
  }
};
