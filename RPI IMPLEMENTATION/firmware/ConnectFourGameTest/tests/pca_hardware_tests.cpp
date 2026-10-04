#include "../PcaHardware.h"
#include "../Controller.h"
#include <assert.h>
#include <stdio.h>
static void resetBus() {
  Wire = TwoWire{}; pwmWrites.clear(); oeLevel = HIGH;
  driverBeginOK = pwmWriteOK = frequencyOK = true;
}
struct QuietEvents : GameEvents {
  uint32_t time = 0;
  void message(const char *) override {}
  void boardChanged(const Game::Board &) override {}
  void searchDone(const AI::SearchResult &, uint32_t) override {}
  uint32_t now() override { return time; }
};
struct ClearSensors : SensorPort {
  uint32_t reset(bool, bool levels[7]) override { for (int c = 0; c < 7; ++c) levels[c] = false; return 0; }
  bool pop(SensorEdge &) override { return false; }
  bool overflowed() override { return false; }
  bool sealClear() override { return true; }
};
int main() {
  resetBus(); PcaHardware io;
  assert(io.begin() && oeLevel == HIGH && pwmWrites.size() == 16);
  assert(Wire.sda == 21 && Wire.scl == 22 && Wire.timeout == 50);
  assert(oscillator == 25000000 && configuredFrequency == 50);
  for (int c = 0; c < 16; ++c) assert(pwmWrites[c].channel == c && pwmWrites[c].on == 0 && pwmWrites[c].off == 4096);
  assert(io.writeAngle(0,0) && pwmWrites.back().off == 102);
  assert(io.writeAngle(6,90) && pwmWrites.back().off == 307);
  assert(io.writeAngle(7,90) && pwmWrites.back().off == 307);
  assert(io.writeAngle(7,0) && pwmWrites.back().off == 102);
  assert(!io.writeAngle(8,90) && !io.writeAngle(0,181));
  for (uint8_t channel = 2; channel <= 4; ++channel) {
    assert(io.writeAngle(channel,0) && pwmWrites.back().off == 4096);
    assert(io.writeAngle(channel,90) && pwmWrites.back().off == 4096);
  }
  // Gameplay owns 0-7; all magazine diagnostics preserve isolated hatch channels.
  HatchSequence hatches(io); QuietEvents events; ClearSensors port; SensorService sensors(port);
  Controller game(hatches,sensors,events);
  game.restart(); game.command("confirm-clear",0); game.command("free",0); game.command("easy",0); game.command("1",0);
  for (uint32_t t = 0; t < 3000; ++t) { events.time = t; game.tick(t); }
  for (size_t i = 16; i < pwmWrites.size(); ++i) assert(pwmWrites[i].channel < 8);
  for (const auto &write : pwmWrites) if (write.channel >= 2 && write.channel <= 4) assert(write.off == 4096);
  io.enable(true); assert(oeLevel == LOW);
  Wire.ack = false; assert(!io.healthy()); game.fault(); assert(oeLevel == HIGH && game.phase == Phase::Fault);
  Wire.ack = true; game.restart(); assert(game.phase == Phase::Fault && oeLevel == HIGH);
  for (int failure = 0; failure < 6; ++failure) {
    resetBus(); PcaHardware bad;
    if (failure == 0) Wire.beginOK = false;
    if (failure == 1) driverBeginOK = false;
    if (failure == 2) Wire.ack = false;
    if (failure == 3) Wire.shortRead = true;
    if (failure == 4) pwmWriteOK = false;
    if (failure == 5) frequencyOK = false;
    assert(!bad.begin() && oeLevel == HIGH);
  }
  resetBus(); PcaHardware corruption; assert(corruption.begin());
  Wire.registers[0xfe] = 122; assert(!corruption.healthy());
  Wire.registers[0xfe] = 121; Wire.registers[0] = 0x10; assert(!corruption.healthy());
  Wire.registers[0] = 0; Wire.shortRead = true; assert(!corruption.healthy());
  resetBus(); PcaHardware writeFailure; assert(writeFailure.begin());
  HatchSequence sequence(writeFailure); sequence.begin(0x7f,0);
  pwmWriteOK = false; sequence.tick(0); assert(sequence.faulted && oeLevel == HIGH);
  puts("PASS: PCA initialization, full-off channels, pulse conversion, configuration/read/write faults and OE disabling.");
}
