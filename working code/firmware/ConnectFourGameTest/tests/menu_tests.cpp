#define MENU_TEST_ONLY 1
#define main existing_native_main
#include "native_tests.cpp"
#undef main
#include "../Buttons.h"
int main() {
  Fixture f; MenuButtons buttons;
  f.game.command("confirm-clear",0);
  auto poll = [&](int button, unsigned duration) {
    while (duration--) {
      f.clock(); buttons.poll(button == 0,button == 1,button == 2,f.game,f.time++);
    }
  };
  auto click = [&](int button) { poll(button,60); poll(-1,40); };
  poll(-1,40);
  click(1); assert(f.game.highlightedMode == Mode::Coach);
  click(2); assert(f.game.phase == Phase::ModeSelect);
  click(1); click(2); assert(f.game.phase == Phase::Difficulty);
  click(0); assert(f.game.difficulty == 2);
  click(1); assert(f.game.difficulty == 0);
  click(2); assert(f.game.phase == Phase::FirstPlayer);
  click(1); assert(f.game.robotFirst);
  click(0); assert(!f.game.robotFirst);
  click(2); assert(f.game.phase == Phase::ModeSelect);
  assert(f.io.writes.empty() && f.game.moveCount == 0);
  f.game.command("coach",f.time);
  assert(f.game.phase == Phase::ModeSelect && f.io.writes.empty());
  puts("PASS: menu-only button navigation, confirmation, repeat cycle and gameplay exclusion.");
}
