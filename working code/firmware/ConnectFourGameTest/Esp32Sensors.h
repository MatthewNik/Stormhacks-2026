#pragma once
#include <Arduino.h>
#include "Sensors.h"

// IR inputs are sampled from a fixed-rate hardware timer instead of per-edge
// GPIO interrupts. A comparator that oscillates near its threshold (disc
// partly in the beam, servo current on shared ground) would otherwise raise
// an unbounded interrupt rate on the loop core and starve the I2C driver until
// a PCA transaction times out. Sampling keeps the IR cost fixed regardless of
// line noise.
class Esp32Sensors : public SensorPort {
  struct Column {
    bool logical = false;   // last level placed in the queue, or the reset sample
    bool raw = false;       // latest sampled level
    uint32_t rawSince = 0;  // when raw last changed
  } columns[7];
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  SensorEdge queue[Config::EDGE_QUEUE_SIZE];
  unsigned head = 0, tail = 0, count = 0;
  bool capture = false, overflow = false;
  hw_timer_t *timer = nullptr;
  bool level(uint8_t c) const {
    const bool high = digitalRead(Config::SENSOR_PINS[c]) == HIGH;
    return Config::SENSOR_ACTIVE_LOW[c] ? !high : high;
  }
  void enqueue(uint8_t column, bool active, uint32_t at) {
    if (count == Config::EDGE_QUEUE_SIZE) { overflow = true; return; }
    queue[head] = {column,active,at};
    head = (head+1)%Config::EDGE_QUEUE_SIZE; ++count;
  }
  // A level enters the queue only after it stops changing. Brief comparator
  // chatter is discarded; the queued timestamp is the start of the stable level.
  void promote(uint32_t now) {
    if (!capture || overflow) return;
    for (uint8_t c = 0; c < 7; ++c) {
      Column &col = columns[c];
      if (col.raw == col.logical) continue;
      if (uint32_t(now-col.rawSince) < Config::SENSOR_DEBOUNCE_US) continue;
      enqueue(c,col.raw,col.rawSince);
      if (overflow) return;
      col.logical = col.raw;
    }
  }
  static void ARDUINO_ISR_ATTR onTimer(void *arg) {
    static_cast<Esp32Sensors *>(arg)->sample(micros());
  }
public:
  void begin() {
    for (uint8_t c = 0; c < 7; ++c) pinMode(Config::SENSOR_PINS[c],INPUT);
    timer = timerBegin(1000000);
    if (!timer) return;
    timerAttachInterruptArg(timer,onTimer,this);
    timerAlarm(timer,Config::SENSOR_SAMPLE_US,true,0);
  }
  bool running() const { return timer != nullptr; }
  void sample(uint32_t now) {
    portENTER_CRITICAL_ISR(&mux);
    if (capture && !overflow) {
      for (uint8_t c = 0; c < 7 && !overflow; ++c) {
        const bool active = level(c);
        Column &col = columns[c];
        if (active == col.raw) continue;
        if (uint32_t(now-col.rawSince) >= Config::SENSOR_DEBOUNCE_US && col.raw != col.logical) {
          enqueue(c,col.raw,col.rawSince);
          if (overflow) break;
          col.logical = col.raw;
        }
        col.raw = active; col.rawSince = now;
      }
      promote(now);
    }
    portEXIT_CRITICAL_ISR(&mux);
  }
  uint32_t reset(bool enabled, bool levels[7]) override {
    portENTER_CRITICAL(&mux);
    capture = false; head = tail = count = 0; overflow = false;
    const uint32_t stamp = micros();
    for (uint8_t c = 0; c < 7; ++c) {
      const bool active = enabled && level(c);
      levels[c] = active;
      columns[c].logical = columns[c].raw = active;
      columns[c].rawSince = stamp;
    }
    capture = enabled;
    portEXIT_CRITICAL(&mux); return stamp;
  }
  bool pop(SensorEdge &edge) override {
    portENTER_CRITICAL(&mux);
    promote(micros());
    const bool found = count != 0;
    if (found) { edge = queue[tail]; tail = (tail+1)%Config::EDGE_QUEUE_SIZE; --count; }
    portEXIT_CRITICAL(&mux); return found;
  }
  bool overflowed() override {
    portENTER_CRITICAL(&mux); const bool value = overflow; portEXIT_CRITICAL(&mux); return value;
  }
  bool sealClear() override {
    portENTER_CRITICAL(&mux);
    promote(micros());
    bool okay = capture && !overflow && count == 0;
    for (uint8_t c = 0; c < 7 && okay; ++c)
      okay = !level(c) && !columns[c].raw && columns[c].raw == columns[c].logical;
    if (okay) capture = false;
    portEXIT_CRITICAL(&mux); return okay;
  }
};
