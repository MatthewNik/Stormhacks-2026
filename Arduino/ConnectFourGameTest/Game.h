#pragma once
#include <stdint.h>

namespace Game {
constexpr int ROWS = 6, COLS = 7;
enum class Result { Playing, OWins, XWins, Draw };
struct Board {
  char cells[ROWS][COLS];
  Board() { clear(); }
  void clear() { for (auto &row : cells) for (char &cell : row) cell = '.'; }
  bool legal(int col) const { return col >= 0 && col < COLS && cells[0][col] == '.'; }
  bool drop(int col, char symbol) {
    if (!legal(col) || (symbol != 'O' && symbol != 'X')) return false;
    for (int r = ROWS - 1; r >= 0; --r) if (cells[r][col] == '.') {
      cells[r][col] = symbol; return true;
    }
    return false;
  }
  uint8_t playableMask() const {
    uint8_t mask = 0;
    for (int c = 0; c < COLS; ++c) if (legal(c)) mask |= uint8_t(1 << c);
    return mask;
  }
  Result result() const {
    const int directions[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    for (int r = 0; r < ROWS; ++r) for (int c = 0; c < COLS; ++c) {
      const char symbol = cells[r][c];
      if (symbol == '.') continue;
      for (const auto &d : directions) {
        int rr = r + 3*d[0], cc = c + 3*d[1];
        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) continue;
        bool win = true;
        for (int i = 1; i < 4; ++i) if (cells[r+i*d[0]][c+i*d[1]] != symbol) win = false;
        if (win) return symbol == 'O' ? Result::OWins : Result::XWins;
      }
    }
    return playableMask() ? Result::Playing : Result::Draw;
  }
};
inline char other(char symbol) { return symbol == 'O' ? 'X' : 'O'; }
}
