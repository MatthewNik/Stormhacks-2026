#pragma once
#include <Arduino.h>
#include <esp_timer.h>
#include "Sensors.h"

// Periodic sampler instead of CHANGE interrupts: LM393 modules have no hysteresis and chatter
// at the threshold, which flooded a per-edge queue. Only debounced transitions are queued.
// The esp_timer task runs on core 0, so OLED redraws and robot search cannot stall sampling.
class Esp32Sensors : public SensorPort {
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  esp_timer_handle_t timer = nullptr;
  SensorEdge queue[Config::EDGE_QUEUE_SIZE];
  unsigned head = 0, tail = 0, count = 0;
  bool capture = false, overflow = false;
  uint32_t armedUs = 0;
  // Debounced level, and the start/length of a run of samples that disagree with it.
  bool stable[7] = {};
  uint32_t runStartUs[7] = {};
  uint8_t runCount[7] = {};
  bool level(uint8_t c) const {
    const bool high = digitalRead(Config::SENSOR_PINS[c]) == HIGH;
    return Config::SENSOR_ACTIVE_LOW[c] ? !high : high;
  }
  static void tick(void *arg) { static_cast<Esp32Sensors *>(arg)->sample(micros()); }
public:
  bool begin() {
    // Seed the debounced levels before the timer starts; nothing else touches them yet.
    for (uint8_t c = 0; c < 7; ++c) {
      pinMode(Config::SENSOR_PINS[c],INPUT); stable[c] = level(c); runCount[c] = 0;
    }
    esp_timer_create_args_t args = {};
    args.callback = tick; args.arg = this; args.name = "ir_sample";
    return esp_timer_create(&args,&timer) == ESP_OK &&
      esp_timer_start_periodic(timer,Config::SENSOR_SAMPLE_US) == ESP_OK;
  }
  // One sample of all columns; public so native tests can drive it with a fake clock.
  void sample(uint32_t nowUs) {
    bool raw[7];
    for (uint8_t c = 0; c < 7; ++c) raw[c] = level(c);
    portENTER_CRITICAL(&mux);
    for (uint8_t c = 0; c < 7; ++c) {
      if (raw[c] == stable[c]) { runCount[c] = 0; continue; }
      if (runCount[c] == 0) runStartUs[c] = nowUs;
      if (++runCount[c] < Config::SENSOR_STABLE_SAMPLES) continue;
      stable[c] = raw[c]; runCount[c] = 0;
      if (!capture) continue;
      if (count == Config::EDGE_QUEUE_SIZE) { overflow = true; continue; }
      // A run that began before arming is stamped at the arming time, never earlier.
      const uint32_t at = int32_t(runStartUs[c]-armedUs) < 0 ? armedUs : runStartUs[c];
      queue[head] = {c,raw[c],at};
      head = (head+1)%Config::EDGE_QUEUE_SIZE; ++count;
    }
    portEXIT_CRITICAL(&mux);
  }
  uint32_t reset(bool enabled, bool levels[7]) override {
    portENTER_CRITICAL(&mux);
    capture = false; head = tail = count = 0; overflow = false;
    const uint32_t stamp = micros();
    for (uint8_t c = 0; c < 7; ++c) levels[c] = enabled && stable[c];
    armedUs = stamp; capture = enabled;
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
    // A pending run means an input is changing and has not settled yet.
    for (uint8_t c = 0; c < 7 && okay; ++c) okay = !stable[c] && runCount[c] == 0;
    if (okay) capture = false;
    portEXIT_CRITICAL(&mux); return okay;
  }
};
