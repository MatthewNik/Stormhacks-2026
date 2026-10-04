#include "../Esp32Sensors.h"
#include <assert.h>
#include <stdio.h>
static void edge(uint8_t col, bool active, uint32_t time) {
  const int pin = Config::SENSOR_PINS[col]; fakeMicros = time;
  pinLevels[pin] = active ? LOW : HIGH; fireTimer();
}
static void settle(uint32_t time) { fakeMicros = time; }
struct Observer : SensorObserver {
  unsigned detects = 0, passages = 0, errors = 0; uint32_t start = 0;
  void detected(uint8_t, uint32_t at) override { ++detects; start = at; }
  void passage(uint8_t, uint32_t at) override { ++passages; assert(at == start); }
  void sensorProblem(const char *) override { ++errors; }
};
int main() {
  for (int &level : pinLevels) level = HIGH;
  {
    timerAvailable = false; Esp32Sensors missing; missing.begin();
    assert(!missing.running()); timerAvailable = true;
  }
  Esp32Sensors source; source.begin(); bool levels[7]; SensorEdge event;
  assert(source.running() && timerArg == &source && fakeTimer.frequency == 1000000);
  assert(timerAlarmTicks == Config::SENSOR_SAMPLE_US && timerAutoReload);
  assert(Config::SENSOR_DEBOUNCE_US >= 4*Config::SENSOR_SAMPLE_US);
  const int wiredPins[7] = {34,35,36,39,32,33,27};
  fakeMicros = 0; source.reset(true,levels);
  for (uint8_t c = 0; c < 7; ++c) {
    const int pin = wiredPins[c];
    assert(pinModes[pin] == INPUT+1);
    const uint32_t started = 1000u+c;
    edge(c,true,started);
    settle(started+Config::SENSOR_DEBOUNCE_US);
    assert(source.pop(event) && event.column == c && event.active && event.atUs == started);
    edge(c,false,started+Config::SENSOR_DEBOUNCE_US);
    settle(started+2*Config::SENSOR_DEBOUNCE_US);
    assert(source.pop(event) && event.column == c && !event.active);
    pinLevels[pin] = HIGH;
  }
  source.reset(false,levels); const auto reads = pinReads;
  edge(0,true,100); edge(0,false,200);
  assert(pinReads == reads && !source.pop(event));
  fakeMicros = 1000; source.reset(true,levels);
  for (bool active : levels) assert(!active);
  edge(3,true,2000); edge(3,false,2000+5000);
  assert(!source.sealClear());
  assert(source.pop(event) && event.column == 3 && event.active && event.atUs == 2000);
  settle(2000+5000+Config::SENSOR_DEBOUNCE_US);
  assert(source.pop(event) && !event.active && event.atUs == 2000+5000);
  assert(source.sealClear()); const auto disarmedReads = pinReads;
  edge(1,true,20000); assert(pinReads == disarmedReads && !source.pop(event));
  pinLevels[Config::SENSOR_PINS[1]] = LOW; fakeMicros = 21000;
  source.reset(true,levels); assert(levels[1]);
  assert(!source.pop(event) && !source.sealClear());
  edge(1,false,22000);
  fakeMicros = 23000; source.reset(true,levels); assert(!source.pop(event));
  // Fast comparator chatter must not overflow or become a passage.
  fakeMicros = 100000; source.reset(true,levels);
  for (unsigned i = 0; i < 500; ++i) edge(0,i%2==0,100000+i*50);
  assert(!source.overflowed());
  unsigned chatter = 0; while (source.pop(event)) ++chatter;
  assert(chatter == 0);
  // Work per sample is fixed: seven pin reads, however fast a line toggles.
  { const auto before = pinReads; fireTimer(); assert(pinReads-before == 7); }
  const uint32_t blocked = 200000;
  edge(2,true,blocked); settle(blocked+Config::SENSOR_DEBOUNCE_US);
  assert(source.pop(event) && event.column == 2 && event.active && event.atUs == blocked);
  assert(!source.pop(event) && !source.overflowed());
  // A pulse that finishes before the main loop still yields both stable edges.
  const uint32_t pulse = 300000;
  edge(4,true,pulse); edge(4,false,pulse+5000);
  settle(pulse+5000+Config::SENSOR_DEBOUNCE_US);
  assert(source.pop(event) && event.column == 4 && event.active && event.atUs == pulse);
  assert(source.pop(event) && !event.active && event.atUs == pulse+5000);
  // Two columns that both stay blocked remain two events.
  const uint32_t both = 400000;
  edge(0,true,both); edge(5,true,both+100);
  settle(both+100+Config::SENSOR_DEBOUNCE_US);
  bool seen[7] = {};
  while (source.pop(event)) { assert(event.active); seen[event.column] = true; }
  assert(seen[0] && seen[5]);
  // Stable transitions still fill and overflow the queue.
  for (int &level : pinLevels) level = HIGH;
  fakeMicros = 500000; source.reset(true,levels); assert(!source.overflowed());
  uint32_t at = 500000; bool active = true;
  for (unsigned i = 0; i < Config::EDGE_QUEUE_SIZE+2; ++i) {
    at += Config::SENSOR_DEBOUNCE_US; edge(0,active,at); active = !active;
  }
  assert(source.overflowed() && !source.sealClear());
  unsigned count = 0; while (source.pop(event)) ++count;
  assert(count == Config::EDGE_QUEUE_SIZE);
  source.reset(false,levels); assert(!source.overflowed() && !source.pop(event));
  // One noisy human passage qualifies once. A later separated passage does not merge away.
  {
    Esp32Sensors port; port.begin(); SensorService service(port); Observer observer;
    for (int &level : pinLevels) level = HIGH;
    fakeMicros = 0; service.reset(true);
    uint32_t t = 10000;
    for (int i = 0; i < 80; ++i) { edge(3,true,t); t += 40; edge(3,false,t); t += 40; }
    service.poll(t,observer);
    assert(observer.errors == 0 && observer.detects == 0 && !port.overflowed());
    for (int burst = 0; burst < 4; ++burst) {
      edge(3,true,t); t += 5000; edge(3,false,t); t += 5000;
    }
    edge(3,true,t); t += 5000; edge(3,false,t);
    const uint32_t clearAt = t;
    t += Config::SENSOR_DEBOUNCE_US; settle(t); service.poll(t,observer);
    assert(observer.errors == 0 && observer.detects == 1 && observer.passages == 0);
    t = clearAt+Config::CLEAR_US; settle(t); service.poll(t,observer);
    assert(observer.errors == 0 && observer.detects == 1 && observer.passages == 1 && observer.start == 10000+80*80);
    t = clearAt+Config::STABLE_CLEAR_US; settle(t);
    assert(service.seal(t,observer) && observer.errors == 0 && !service.enabled);
    service.reset(true);
    Observer again; edge(3,true,t+1000); edge(3,false,t+8000);
    uint32_t done = t+8000+Config::CLEAR_US; settle(done); service.poll(done,again);
    edge(3,true,done+1000); edge(3,false,done+8000);
    done = done+8000+Config::CLEAR_US; settle(done); service.poll(done,again);
    assert(again.detects == 2 && again.passages == 2 && again.errors == 0);
  }
  puts("PASS: timer-sampled IR adapter gating, GPIO map, debounce, chatter, bounded queue, sealing and epoch reset.");
}
