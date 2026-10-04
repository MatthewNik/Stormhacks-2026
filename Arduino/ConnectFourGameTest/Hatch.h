#pragma once
#include "GameConfig.h"
struct HatchIO {
  virtual ~HatchIO() = default;
  virtual bool writeAngle(uint8_t channel, uint16_t angle) = 0;
  virtual bool stopChannel(uint8_t channel) = 0;
  virtual void enable(bool enabled) = 0;
};
// Sole owner of OE and all eight deadlines. Hatch batches never reset PCA7.
class HatchSequence {
  HatchIO &io;
  bool driving[8] = {};
  uint32_t started[8] = {}, limits[8] = {};
  uint8_t mask = 0, next = 0;
  uint32_t last = 0;
  bool active = false, first = false, settling = false;
  void updateEnable() {
    bool any = false; for (bool value : driving) any |= value;
    io.enable(any && !faulted);
  }
public:
  bool faulted = false;
  explicit HatchSequence(HatchIO &hardware) : io(hardware) {}
  void disable() {
    active = false; io.enable(false);
    for (bool &value : driving) value = false;
    for (uint8_t c = 0; c < 8; ++c) if (!io.stopChannel(c)) faulted = true;
  }
  void fault() { faulted = true; disable(); }
  bool command(uint8_t channel, uint16_t angle, uint32_t now, uint32_t duration = Config::SERVO_DRIVE_MS) {
    if (faulted || channel >= 8 || !duration) return false;
    if (!io.writeAngle(channel,angle)) { fault(); return false; }
    driving[channel] = channel == 7 || Config::HATCH_ENABLED[channel];
    started[channel] = now; limits[channel] = duration; updateEnable(); return true;
  }
  void begin(uint8_t openMask, uint32_t now) {
    if (faulted) return;
    mask = openMask & 0x7f; next = 0; last = now;
    active = first = true; settling = false;
  }
  bool busy() const { return active; }
  bool openTarget(uint8_t column, uint32_t now) {
    return column < 7 && command(column,Config::HATCHES[column].open,now,Config::ROBOT_DRIVE_MS);
  }
  void tick(uint32_t now) {
    if (faulted) return;
    for (uint8_t c = 0; c < 8; ++c) if (driving[c] && uint32_t(now-started[c]) >= limits[c]) {
      if (!io.stopChannel(c)) { fault(); return; }
      driving[c] = false;
    }
    updateEnable();
    if (!active) return;
    if (settling) { if (uint32_t(now-last) >= Config::SETTLE_MS) active = false; return; }
    if (!first && uint32_t(now-last) < Config::COMMAND_GAP_MS) return;
    const auto &cal = Config::HATCHES[next];
    if (!command(next,(mask & (1 << next)) ? cal.open : cal.closed,now)) return;
    first = false; last = now;
    if (++next == 7) settling = true;
  }
};
