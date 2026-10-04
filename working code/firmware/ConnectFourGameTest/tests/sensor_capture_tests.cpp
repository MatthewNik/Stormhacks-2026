#include "../Esp32Sensors.h"
#include <assert.h>
#include <stdio.h>
static uint32_t clockUs = 1000;
static void set(uint8_t col, bool active) { pinLevels[Config::SENSOR_PINS[col]] = active ? LOW : HIGH; }
// Advance the fake clock and fire the registered esp_timer callback once per sample period.
static void step(unsigned samples = 1) {
  for (unsigned i = 0; i < samples; ++i) {
    clockUs += Config::SENSOR_SAMPLE_US; fakeMicros = clockUs; timerCallback(timerArg);
  }
}
static void settle(unsigned samples = Config::SENSOR_STABLE_SAMPLES) { step(samples); }
// Alternate the input every sample: LM393 threshold chatter at the sampling rate.
static void chatter(uint8_t col, unsigned samples, bool startActive) {
  for (unsigned i = 0; i < samples; ++i) { set(col,(i%2 == 0) == startActive); step(); }
}
struct Recorder : SensorObserver {
  int detections = 0, passages = 0; uint8_t column = 255; const char *problem = nullptr;
  void detected(uint8_t c, uint32_t) override { ++detections; column = c; }
  void passage(uint8_t, uint32_t) override { ++passages; }
  void sensorProblem(const char *reason) override { problem = reason; }
};
int main() {
  for (int &level : pinLevels) level = HIGH;
  fakeMicros = clockUs;
  { timerCreateOK = false; Esp32Sensors failing; assert(!failing.begin()); timerCreateOK = true; }
  Esp32Sensors source; assert(source.begin());
  assert(timerCallback && timerArg == &source && timerPeriodUs == Config::SENSOR_SAMPLE_US);
  for (int pin = 0; pin < 40; ++pin) assert(!interrupts[pin]); // no GPIO interrupts at all
  bool levels[7]; SensorEdge event;

  // Drive the physical wiring pins, not the configuration array, to verify every column association.
  const int wiredPins[7] = {34,35,36,39,32,33,27};
  source.reset(true,levels);
  for (uint8_t c = 0; c < 7; ++c) {
    pinLevels[wiredPins[c]] = LOW; const uint32_t first = clockUs + Config::SENSOR_SAMPLE_US;
    settle();
    assert(source.pop(event) && event.column == c && event.active && event.atUs == first);
    pinLevels[wiredPins[c]] = HIGH; settle();
    assert(source.pop(event) && event.column == c && !event.active);
    assert(!source.pop(event));
  }

  // A level must hold for SENSOR_STABLE_SAMPLES consecutive samples.
  source.reset(true,levels);
  set(2,true); step(Config::SENSOR_STABLE_SAMPLES-1); set(2,false); step(20);
  assert(!source.pop(event));
  set(2,true); settle(); assert(source.pop(event) && event.column == 2 && event.active);
  set(2,false); settle(); assert(source.pop(event) && !event.active);

  // Regression: threshold chatter used to flood the per-edge ISR queue. It must produce nothing now.
  source.reset(true,levels);
  chatter(0,2000,true);
  for (unsigned i = 0; i < 500; ++i) { set(5,i%4 != 3); step(); } // 75% active duty, never 2 ms solid
  set(0,false); set(5,false); settle();
  assert(!source.pop(event) && !source.overflowed());

  // Sealing requires settled inputs: a pending run blocks it even before it is debounced.
  set(3,true); step(); assert(!source.sealClear());
  set(3,false); step(); assert(source.sealClear());
  set(1,true); settle(); assert(!source.pop(event)); // disarmed: tracked but not queued

  // An input already debounced active at arming is a snapshot, not an edge.
  source.reset(true,levels); assert(levels[1]);
  assert(!source.pop(event) && !source.sealClear());
  set(1,false); settle(); assert(source.pop(event) && event.column == 1 && !event.active);
  assert(source.sealClear());

  // A run that began before arming is stamped no earlier than the arming time.
  source.reset(false,levels); set(2,true); step(2);
  const uint32_t armed = source.reset(true,levels); assert(!levels[2]);
  step(2); assert(source.pop(event) && event.column == 2 && event.active && event.atUs == armed);
  set(2,false); settle();
  source.reset(true,levels); assert(!source.pop(event)); // reset discards the prior epoch

  // Debounced transitions are still bounded: a genuinely oscillating input overflows and blocks sealing.
  for (unsigned i = 0; i <= Config::EDGE_QUEUE_SIZE; ++i) {
    set(0,i%2 == 0); settle();
    assert(source.overflowed() == (i == Config::EDGE_QUEUE_SIZE));
  }
  assert(!source.sealClear());
  set(0,false); settle();
  unsigned count = 0; while (source.pop(event)) ++count;
  assert(count == Config::EDGE_QUEUE_SIZE);
  source.reset(false,levels); assert(!source.overflowed() && !source.pop(event));

  // End to end: a chattering human-style passage, with the main loop stalled for the whole
  // passage (~110 ms, like an OLED redraw), still yields exactly one detection and one passage.
  {
    Esp32Sensors fresh; assert(fresh.begin());
    SensorService service(fresh); Recorder seen; service.reset(true);
    chatter(4,30,true); set(4,true); step(60); chatter(4,30,false); set(4,false); step(100);
    service.poll(clockUs,seen);
    assert(!seen.problem && seen.detections == 1 && seen.passages == 1 && seen.column == 4);
    step(Config::STABLE_CLEAR_US/Config::SENSOR_SAMPLE_US);
    assert(service.seal(clockUs,seen) && !seen.problem);
  }
  puts("PASS: sampled IR adapter GPIO map, debounce, chatter rejection, sealing, epoch reset, bounded queue, stalled-loop passage.");
}
