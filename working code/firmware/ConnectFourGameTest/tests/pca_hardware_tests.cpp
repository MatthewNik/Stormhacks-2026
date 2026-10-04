#include "../PcaHardware.h"
#include "../Controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
static void resetBus() {
  Wire = TwoWire{}; pwmWrites.clear(); oeLevel = HIGH;
  driverBeginOK = pwmWriteOK = frequencyOK = true;
  pwmFailNext = 0;
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
  assert(Wire.frequency == 0 && Wire.repeatedReads == 3 && Wire.stoppedSelections == 0);
  assert(oscillator == 25000000 && configuredFrequency == 50);
  for (int c = 0; c < 16; ++c) assert(pwmWrites[c].channel == c && pwmWrites[c].on == 0 && pwmWrites[c].off == 4096);
  assert(io.writeAngle(0,0) && pwmWrites.back().off == 102);
  assert(io.writeAngle(6,90) && pwmWrites.back().off == 307);
  assert(io.writeAngle(7,90) && pwmWrites.back().off == 307);
  assert(io.writeAngle(7,0) && pwmWrites.back().off == 102);
  assert(!io.writeAngle(8,90) && !io.writeAngle(0,181));
  for (uint8_t channel = 2; channel <= 4; ++channel) {
    assert(io.writeAngle(channel,0) && pwmWrites.back().off == 102);
    assert(io.writeAngle(channel,90) && pwmWrites.back().off == 307);
  }
  // Gameplay owns 0-7; calibrated hatch channels now receive position pulses.
  HatchSequence hatches(io); QuietEvents events; ClearSensors port; SensorService sensors(port);
  Controller game(hatches,sensors,events);
  game.restart(); game.command("confirm-clear",0);
  for (uint32_t t = 0; t <= 650; ++t) { events.time = t; game.tick(t); }
  game.command("free",651); game.command("easy",0); game.command("1",0);
  for (uint32_t t = 0; t < 3000; ++t) { events.time = t; game.tick(t); }
  for (size_t i = 16; i < pwmWrites.size(); ++i) assert(pwmWrites[i].channel < 8);
  for (uint8_t channel = 2; channel <= 4; ++channel) {
    bool moved = false;
    for (const auto &write : pwmWrites) if (write.channel == channel && write.off != 4096) moved = true;
    assert(moved);
  }
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
    const char *expected[] = {"I2C initialization", "Adafruit driver initialization", "address probe",
                             "register 0xFE read", "startup FULL_OFF", "PWM frequency"};
    assert(strstr(bad.error(),expected[failure]));
    assert(strstr(bad.error(),"address=0x40 SDA=21 SCL=22"));
    const std::string firstError = bad.error();
    pwmWriteOK = false; HatchSequence shutdown(bad); shutdown.fault();
    assert(firstError == bad.error() && oeLevel == HIGH);
  }
  resetBus(); PcaHardware corruption; assert(corruption.begin());
  Wire.registers[0xfe] = 122; assert(!corruption.healthy());
  Wire.registers[0xfe] = 121; Wire.registers[0] = 0x10; assert(!corruption.healthy());
  Wire.registers[0] = 0; Wire.shortRead = true; assert(!corruption.healthy());
  resetBus(); PcaHardware writeFailure; assert(writeFailure.begin());
  HatchSequence sequence(writeFailure); sequence.begin(0x7f,0);
  pwmWriteOK = false; sequence.tick(0); assert(sequence.faulted && oeLevel == HIGH);
  assert(strstr(writeFailure.error(),"motor position write failed"));
  resetBus(); PcaHardware asleep; assert(asleep.begin()); Wire.registers[0] = 0x10;
  assert(!asleep.healthy() && strstr(asleep.error(),"sleep mode"));
  resetBus(); PcaHardware changed; assert(changed.begin()); Wire.registers[0xfe] = 122;
  assert(!changed.healthy() && strstr(changed.error(),"PWM prescaler changed"));
  // Reproduce the reported empty combined read while address probing still succeeds.
  resetBus(); Wire.rejectRepeatedStart = true;
  Wire.beginTransmission(Config::PCA_ADDRESS); Wire.write(uint8_t(0xFE));
  assert(Wire.endTransmission(false) == 0 && Wire.requestFrom(Config::PCA_ADDRESS,size_t(1),true) == 0);
  const auto combinedReads = Wire.repeatedReads;
  PcaHardware compatible; assert(compatible.begin());
  assert(Wire.frequency == 0 && Wire.stoppedSelections >= 3);
  assert(Wire.repeatedReads == combinedReads+3 && compatible.healthy());
  assert(compatible.writeAngle(0,115) && pwmWrites.back().off != 4096);
  Wire.pointerAck = false;
  assert(!compatible.healthy() && strstr(compatible.error(),"register 0xFE select failed"));
  resetBus(); Wire.emptyRead = true; PcaHardware missingByte;
  assert(!missingByte.begin() && strstr(missingByte.error(),"register 0xFE byte missing"));
  // One transient write NACK is retried; two in a row still fail the write.
  resetBus(); PcaHardware retrying; assert(retrying.begin());
  pwmFailNext = 1; assert(retrying.writeAngle(0,0) && pwmWrites.back().off == 102 && pwmFailNext == 0);
  pwmFailNext = 1; assert(retrying.stopChannel(3) && pwmWrites.back().off == 4096);
  pwmFailNext = 2; assert(!retrying.writeAngle(1,0) && strstr(retrying.error(),"motor position write failed"));
  // Health monitoring tolerates isolated misses but faults on PCA_HEALTH_MISSES consecutive ones.
  resetBus(); PcaHardware monitored; assert(monitored.begin());
  Wire.ack = false;
  for (unsigned i = 1; i < Config::PCA_HEALTH_MISSES; ++i) assert(monitored.monitor() && monitored.misses == i);
  Wire.ack = true; assert(monitored.monitor() && monitored.misses == 0);
  assert(strstr(monitored.error(),"no failure recorded"));
  assert(strstr(monitored.error(),"Health-check misses this boot=2"));
  assert(strstr(monitored.error(),"last recovered: PCA detail: register 0xFE select failed"));
  Wire.ack = false;
  for (unsigned i = 1; i < Config::PCA_HEALTH_MISSES; ++i) assert(monitored.monitor());
  assert(!monitored.monitor());
  assert(strstr(monitored.error(),"register 0xFE select failed") && strstr(monitored.error(),"3 consecutive health misses"));
  assert(strstr(monitored.error(),"Health-check misses this boot=5"));
  // A wrong prescaler (PCA reset/brownout) that persists also faults after the same count.
  resetBus(); PcaHardware rebooted; assert(rebooted.begin()); Wire.registers[0xfe] = 0x1e;
  for (unsigned i = 1; i < Config::PCA_HEALTH_MISSES; ++i) assert(rebooted.monitor());
  assert(!rebooted.monitor() && strstr(rebooted.error(),"PWM prescaler changed"));
  // An already recorded failure is never discarded by a later monitor call.
  resetBus(); PcaHardware latched; assert(latched.begin());
  pwmWriteOK = false; assert(!latched.writeAngle(0,0)); pwmWriteOK = true;
  Wire.ack = false; assert(!latched.monitor() && strstr(latched.error(),"motor position write failed"));
  puts("PASS: PCA initialization, full-off channels, pulse conversion, configuration/read/write faults and OE disabling.");
}
