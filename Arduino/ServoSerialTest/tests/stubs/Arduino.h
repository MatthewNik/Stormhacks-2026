#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <deque>
#include <vector>
#include <tuple>
#include <sstream>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
inline int outputEnable = HIGH;
inline uint32_t clockMs = 0;
inline void digitalWrite(int, int level) { outputEnable = level; }
inline void pinMode(int, int) {}
inline uint32_t millis() { return clockMs; }
inline char *strtok_r(char *s, const char *delim, char **context) {
  return strtok_s(s, delim, context);
}
struct SerialStub {
  std::deque<char> input;
  std::string output;
  void begin(uint32_t) {}
  int available() { return int(input.size()); }
  char read() { char c = input.front(); input.pop_front(); return c; }
  template<class T> void print(T value) {
    std::ostringstream stream; stream << value; output += stream.str();
  }
  void print(uint8_t value) { print(unsigned(value)); }
  void println() { output += "\n"; }
  template<class T> void println(T value) { print(value); println(); }
};
inline SerialStub Serial;
