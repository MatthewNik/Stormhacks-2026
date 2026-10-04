#include "../Esp32Sensors.h"
#include <assert.h>
#include <stdio.h>
static void edge(uint8_t col, bool active, uint32_t time) {
  const int pin = Config::SENSOR_PINS[col]; fakeMicros = time;
  pinLevels[pin] = active ? LOW : HIGH; interrupts[pin](interruptContexts[pin]);
}
int main() {
  for (int &level : pinLevels) level = HIGH;
  Esp32Sensors source; source.begin(); bool levels[7]; SensorEdge event;
  // Drive the physical wiring pins, not the configuration array, to verify every column association.
  const int wiredPins[7] = {34,35,36,39,32,33,27};
  source.reset(true,levels);
  for (uint8_t c = 0; c < 7; ++c) {
    const int pin = wiredPins[c];
    assert(interrupts[pin] && interruptContexts[pin]);
    pinLevels[pin] = LOW; fakeMicros = 10+c;
    interrupts[pin](interruptContexts[pin]);
    assert(source.pop(event) && event.column == c && event.active && event.atUs == uint32_t(10+c));
    pinLevels[pin] = HIGH;
  }
  source.reset(false,levels); const auto reads = pinReads;
  edge(0,true,100); edge(0,false,200);
  assert(pinReads == reads && !source.pop(event));
  source.reset(true,levels);
  for (bool active : levels) assert(!active);
  edge(3,true,1000); edge(3,false,6000);
  assert(!source.sealClear());
  assert(source.pop(event) && event.column == 3 && event.active && event.atUs == 1000);
  assert(source.pop(event) && !event.active && event.atUs == 6000);
  assert(source.sealClear()); const auto disarmedReads = pinReads;
  edge(1,true,7000); assert(pinReads == disarmedReads && !source.pop(event));
  source.reset(true,levels); assert(levels[1]); // initial active snapshot is not an edge
  assert(!source.pop(event) && !source.sealClear()); edge(1,false,8000);
  source.reset(true,levels); assert(!source.pop(event)); // reset discards prior epoch
  for (unsigned i = 0; i < Config::EDGE_QUEUE_SIZE; ++i) edge(0,i%2==0,10000+i);
  assert(!source.overflowed()); edge(0,true,11000); assert(source.overflowed() && !source.sealClear());
  unsigned count = 0; while (source.pop(event)) ++count;
  assert(count == Config::EDGE_QUEUE_SIZE);
  source.reset(false,levels); assert(!source.overflowed() && !source.pop(event));
  puts("PASS: production ISR adapter gating, GPIO map, bounded queue, initial levels, sealing and epoch reset.");
}
