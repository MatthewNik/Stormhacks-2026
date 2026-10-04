#pragma once
#include "GameConfig.h"
struct HatchIO {
  virtual ~HatchIO() = default;
  virtual bool writeAngle(uint8_t channel, uint16_t angle) = 0;
  virtual bool stopChannel(uint8_t channel) = 0;
  virtual void enable(bool enabled) = 0;
  virtual const char *error() const { return "PCA check failed; no driver detail available."; }
};
// Sole owner of OE. Positions hold until changed or explicitly disabled.
class HatchSequence {
  HatchIO &io;
  bool driving[8] = {};
  uint8_t mask = 0, next = 0;
  uint8_t count = 7;
  uint32_t last = 0;
  bool active = false, first = false, settling = false;
  void updateEnable() {
    bool any = false; for (bool value : driving) any |= value;
    io.enable(any && !faulted);
  }
public:
  bool faulted = false;
  const char *error() const { return io.error(); }
  explicit HatchSequence(HatchIO &hardware) : io(hardware) {}
  void disable() {
    active = false; io.enable(false);
    for (bool &value : driving) value = false;
    for (uint8_t c = 0; c < 8; ++c) if (!io.stopChannel(c)) faulted = true;
  }
  void fault() { faulted = true; disable(); }
  bool command(uint8_t channel, uint16_t angle, uint32_t) {
    if (faulted || channel >= 8) return false;
    if (!io.writeAngle(channel,angle)) { fault(); return false; }
    driving[channel] = channel == 7 || Config::HATCH_ENABLED[channel];
    updateEnable(); return true;
  }
  void begin(uint8_t openMask, uint32_t now) {
    if (faulted) return;
    mask = openMask & 0x7f; next = 0; count = 7; last = now;
    active = first = true; settling = false;
  }
  void beginStartup(uint32_t now) { begin(0x7f,now); count = 8; }
  bool busy() const { return active; }
  bool openTarget(uint8_t column, uint32_t now) {
    return column < 7 && command(column,Config::HATCHES[column].open,now);
  }
  void tick(uint32_t now) {
    if (faulted) return;
    updateEnable();
    if (!active) return;
    if (settling) { if (uint32_t(now-last) >= Config::SETTLE_MS) active = false; return; }
    if (!first && uint32_t(now-last) < Config::COMMAND_GAP_MS) return;
    const auto &cal = next == 7 ? Config::INDEXER : Config::HATCHES[next];
    if (!command(next,next == 7 || (mask & (1 << next)) ? cal.open : cal.closed,now)) return;
    first = false; last = now;
    if (++next == count) settling = true;
  }
};
