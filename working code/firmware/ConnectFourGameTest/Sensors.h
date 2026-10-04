#pragma once
#include "GameConfig.h"
struct SensorEdge { uint8_t column; bool active; uint32_t atUs; };
struct SensorPort {
  virtual ~SensorPort() = default;
  virtual uint32_t reset(bool capture, bool levels[7]) = 0;
  virtual bool pop(SensorEdge &edge) = 0;
  virtual bool overflowed() = 0;
  // Atomically disarm only if queue empty, no overflow and every input clear.
  virtual bool sealClear() = 0;
};
struct SensorObserver {
  virtual ~SensorObserver() = default;
  virtual void detected(uint8_t column, uint32_t startUs) = 0;
  virtual void passage(uint8_t column, uint32_t startUs) = 0;
  virtual void sensorProblem(const char *reason) = 0;
};
class SensorService {
  struct State {
    bool active = false, qualified = false, initial = false;
    uint32_t lowStart = 0, changed = 0, detectionStart = 0;
  } states[7];
  SensorPort &port;
  uint32_t generation = 0, activity = 0;
  bool advance(uint32_t at, SensorObserver &observer, uint32_t epoch) {
    for (uint8_t c = 0; c < 7; ++c) {
      auto &s = states[c];
      if (s.active) {
        if (uint32_t(at-s.lowStart) >= Config::STUCK_US) { observer.sensorProblem("Sensor stuck active"); return false; }
        if (!s.initial && !s.qualified && uint32_t(at-s.lowStart) >= Config::DETECT_US) {
          s.qualified = true; s.detectionStart = s.lowStart; observer.detected(c,s.detectionStart);
          if (generation != epoch) return false;
        }
      } else if (s.qualified && uint32_t(at-s.changed) >= Config::CLEAR_US) {
        s.qualified = false; observer.passage(c,s.detectionStart);
        if (generation != epoch) return false;
      }
    }
    return true;
  }
public:
  bool enabled = false;
  explicit SensorService(SensorPort &source) : port(source) {}
  void reset(bool capture) {
    bool levels[7] = {}; activity = port.reset(capture,levels); ++generation; enabled = capture;
    for (uint8_t c = 0; c < 7; ++c) {
      states[c] = State{}; states[c].active = levels[c]; states[c].initial = levels[c];
      states[c].lowStart = states[c].changed = activity;
    }
  }
  // First column currently active or qualified, or -1; used only for diagnostics.
  int activeColumn() const {
    for (int c = 0; c < 7; ++c) if (states[c].active || states[c].qualified) return c;
    return -1;
  }
  bool clear() const {
    for (const auto &s : states) if (s.active || s.qualified) return false;
    return true;
  }
  bool stableClear(uint32_t nowUs) const {
    return clear() && int32_t(nowUs-activity) >= 0 && uint32_t(nowUs-activity) >= Config::STABLE_CLEAR_US;
  }
  void poll(uint32_t nowUs, SensorObserver &observer) {
    if (!enabled) return;
    const uint32_t epoch = generation;
    if (port.overflowed()) { observer.sensorProblem("Sensor edge queue overflow"); return; }
    SensorEdge edge;
    unsigned processed = 0;
    while (port.pop(edge)) {
      if (++processed > Config::EDGE_QUEUE_SIZE || port.overflowed()) {
        observer.sensorProblem("Sensor backlog or overflow"); return;
      }
      if (!advance(edge.atUs,observer,epoch)) return;
      if (edge.column >= 7) { observer.sensorProblem("Invalid sensor event"); return; }
      auto &s = states[edge.column];
      if (edge.active == s.active) continue;
      s.active = edge.active; s.changed = activity = edge.atUs;
      if (edge.active) { if (!s.qualified) s.lowStart = edge.atUs; }
      else s.initial = false;
    }
    if (port.overflowed()) { observer.sensorProblem("Sensor edge queue overflow"); return; }
    // ISR edges can be newer than the caller's sampled clock.
    if (int32_t(nowUs-activity) < 0) nowUs = activity;
    advance(nowUs,observer,epoch);
  }
  bool seal(uint32_t nowUs, SensorObserver &observer) {
    poll(nowUs,observer);
    if (!enabled || !stableClear(nowUs) || !port.sealClear()) return false;
    enabled = false; ++generation; return true;
  }
};
