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
inline void (*interrupts[40])(void *) = {};
inline void *interruptContexts[40] = {};
inline uint32_t micros() { return fakeMicros; }
inline void pinMode(int, int) {}
inline int digitalRead(int pin) { ++pinReads; return pinLevels[pin]; }
inline void attachInterruptArg(int pin, void (*handler)(void *), void *ctx, int) {
  interrupts[pin] = handler; interruptContexts[pin] = ctx;
}
