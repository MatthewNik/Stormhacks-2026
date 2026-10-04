#pragma once
#include <stdint.h>
constexpr int HIGH = 1, LOW = 0;
inline int oeLevel = HIGH;
inline void digitalWrite(int, int value) { oeLevel = value; }
constexpr int INPUT = 0, CHANGE = 3;
#define ARDUINO_ISR_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
inline uint32_t fakeMicros = 0;
inline int pinLevels[40] = {};
inline unsigned pinReads = 0;
inline int pinModes[40] = {};
inline unsigned delayedUs = 0;
inline uint32_t micros() { return fakeMicros; }
inline void delayMicroseconds(unsigned us) { delayedUs += us; }
inline void pinMode(int pin, int mode) { pinModes[pin] = mode+1; }
inline int digitalRead(int pin) { ++pinReads; return pinLevels[pin]; }
struct hw_timer_t { uint32_t frequency; };
inline hw_timer_t fakeTimer{};
inline bool timerAvailable = true;
inline void (*timerCallback)(void *) = nullptr;
inline void *timerArg = nullptr;
inline uint64_t timerAlarmTicks = 0;
inline bool timerAutoReload = false;
inline hw_timer_t *timerBegin(uint32_t frequency) {
  if (!timerAvailable) return nullptr;
  fakeTimer.frequency = frequency; return &fakeTimer;
}
inline void timerAttachInterruptArg(hw_timer_t *, void (*handler)(void *), void *arg) {
  timerCallback = handler; timerArg = arg;
}
inline void timerAlarm(hw_timer_t *, uint64_t ticks, bool reload, uint64_t) {
  timerAlarmTicks = ticks; timerAutoReload = reload;
}
inline void fireTimer() { timerCallback(timerArg); }
