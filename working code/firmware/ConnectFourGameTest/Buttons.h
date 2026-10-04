#pragma once
#include "Controller.h"

enum class ButtonAction { None, Press };
class DebouncedButton {
  bool raw = false, stable = false;
  uint32_t changedAt = 0;
public:
  bool released() const { return !raw && !stable; }
  ButtonAction poll(bool down, uint32_t now) {
    if (down != raw) { raw = down; changedAt = now; }
    if (raw != stable && uint32_t(now-changedAt) >= Config::BUTTON_DEBOUNCE_MS) {
      stable = raw;
      if (stable) return ButtonAction::Press;
    }
    return ButtonAction::None;
  }
};
class MenuButtons {
  DebouncedButton left, right, centre;
  Phase observed = Phase::AwaitClear;
  bool armed = false;
public:
  void poll(bool leftDown, bool rightDown, bool centreDown, Controller &game, uint32_t now) {
    const auto l = left.poll(leftDown,now), r = right.poll(rightDown,now), c = centre.poll(centreDown,now);
    if (observed != game.phase) { observed = game.phase; armed = false; }
    if (int(leftDown)+int(rightDown)+int(centreDown) > 1) { armed = false; return; }
    if (!armed) { if (left.released() && right.released() && centre.released()) armed = true; return; }
    if (game.phase == Phase::ModeSelect) {
      if (l == ButtonAction::Press || r == ButtonAction::Press) {
        game.highlightedMode = game.highlightedMode == Mode::FreePlay ? Mode::Coach : Mode::FreePlay;
        ++game.revision;
      } else if (c == ButtonAction::Press) game.chooseMode(game.highlightedMode,now);
    } else if (game.phase == Phase::Difficulty) {
      if (l == ButtonAction::Press || r == ButtonAction::Press) {
        game.difficulty = (game.difficulty+(l == ButtonAction::Press ? 2 : 1))%3;
        ++game.revision;
      } else if (c == ButtonAction::Press) game.setPhase(Phase::FirstPlayer);
    } else if (game.phase == Phase::FirstPlayer) {
      if (l == ButtonAction::Press || r == ButtonAction::Press) { game.robotFirst = !game.robotFirst; ++game.revision; }
      else if (c == ButtonAction::Press) game.beginPlay(now);
    } else if (game.phase == Phase::Paused && c == ButtonAction::Press) game.resume(now);
    if (observed != game.phase) { observed = game.phase; armed = false; }
  }
};
