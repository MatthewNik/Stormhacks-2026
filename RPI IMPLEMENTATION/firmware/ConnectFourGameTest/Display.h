#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "src/ssd1331/Adafruit_SSD1331.h"
#include "Controller.h"

class GameDisplay {
  Adafruit_SSD1331 oled{&SPI, Config::OLED_CS, Config::OLED_DC, Config::OLED_RESET};
  static constexpr uint16_t WHITE = 0xffff, YELLOW = 0xffe0, CYAN = 0x07ff;
  void text(int x, int y, const char *value, uint16_t color = WHITE) {
    oled.setCursor(x,y); oled.setTextColor(color); oled.print(value);
  }
  void menu(const char *title, const char *selection, const char *secondLine = nullptr) {
    text(0,0,title,CYAN); oled.setTextSize(2);
    text(0,14,selection,YELLOW);
    if (secondLine) text(0,30,secondLine,YELLOW);
    oled.setTextSize(1); text(0,46,"<  Left/Right  >"); text(0,56,"Centre: confirm");
  }
public:
  void begin() {
    digitalWrite(Config::OLED_CS,HIGH); pinMode(Config::OLED_CS,OUTPUT);
    SPI.begin(Config::OLED_CLOCK,-1,Config::OLED_DATA,Config::OLED_CS);
    oled.begin(Config::SPI_HZ); oled.setRotation(0); oled.enableDisplay(true);
    oled.setTextSize(1); oled.setTextWrap(false);
  }
  void render(const Controller &game) {
    oled.fillScreen(0);
    oled.setTextSize(1);
    if (game.phase == Phase::AwaitClear) {
      text(0,0,"CLEAR REQUIRED",YELLOW); text(0,15,"Board + indexer");
      text(0,26,"and feed path"); text(0,42,"confirm-clear",CYAN);
      text(0,55,"Serial Monitor"); return;
    }
    if (game.phase == Phase::ModeSelect) {
      if (game.highlightedMode == Mode::FreePlay) menu("Mode","FREE","PLAY");
      else menu("Mode","COACH");
      return;
    }
    if (game.phase == Phase::FirstPlayer) {
      menu("Who starts?",game.robotFirst ? "ROBOT" : "HUMAN"); return;
    }
    if (game.phase == Phase::Difficulty) {
      menu("Difficulty",game.difficulty == 0 ? "EASY" : game.difficulty == 1 ? "MEDIUM" : "HARD"); return;
    }
    char heading[] = "H:O R:X easy"; heading[2] = game.human; heading[6] = game.robot;
    text(0,0,heading,CYAN);
    // Difficulty occupies the right side of the assignment line.
    oled.fillRect(48,0,48,8,0);
    text(48,0,game.difficulty == 0 ? "easy" : game.difficulty == 1 ? "medium" : game.difficulty == 2 ? "hard" : "--",CYAN);
    for (int c = 0; c < 7; ++c) {
      oled.setCursor(8+c*12,8); oled.setTextColor(YELLOW); oled.print(c+1);
      for (int r = 0; r < 6; ++r) {
        const int x = 8+c*12, y = 16+r*6;
        const char symbol = game.board.cells[r][c];
        if (symbol == 'O') oled.drawCircle(x+2,y+2,2,YELLOW);
        else if (symbol == 'X') {
          oled.drawLine(x,y,x+4,y+4,CYAN); oled.drawLine(x+4,y,x,y+4,CYAN);
        } else oled.drawPixel(x+2,y+2,0x4208);
      }
    }
    const char *status = "Hatches moving";
    switch (game.phase) {
      case Phase::HumanReady: status = "Human: enter 1-7"; break;
      case Phase::HumanOpening: status = "Human: opening"; break;
      case Phase::ClosingForRobot: status = "Robot: closing"; break;
      case Phase::RobotSearch: status = "Robot: thinking"; break;
      case Phase::RobotOpening: status = "Robot: opening"; break;
      case Phase::RobotBaseline: status = "IR: clear check"; break;
      case Phase::IndexerLoading: status = "Indexer: load 90"; break;
      case Phase::IndexerRelease: status = "Indexer: drop 0"; break;
      case Phase::RobotConfirm: status = "Waiting for IR"; break;
      case Phase::RobotQuiet: status = "IR: quiet check"; break;
      case Phase::RobotClosing: status = "Robot: closing"; break;
      case Phase::Paused: status = "PAUSE: correct"; break;
      case Phase::Correction: status = "arm-manual"; break;
      case Phase::ManualBaseline: status = "IR: clear check"; break;
      case Phase::ManualWait: status = "Manual target IR"; break;
      case Phase::AwaitCorrection: status = "Confirm recovery"; break;
      case Phase::Stopped: status = "STOP: restart"; break;
      case Phase::Fault: status = "FAULT: reset"; break;
      case Phase::Ended: case Phase::EndClosing:
        status = game.result == Game::Result::Draw ? "Draw" : game.result == Game::Result::OWins ? "O wins" : "X wins"; break;
      default: break;
    }
    text(0,56,status,WHITE);
    if (game.pendingColumn >= 0) {
      // Pending target stays distinct from the committed board.
      oled.setTextColor(YELLOW);
      if (!Config::HATCH_ENABLED[game.pendingColumn]) { oled.setCursor(78,0); oled.print('!'); }
      oled.setCursor(84,0); oled.print(game.pendingColumn+1);
    }
  }
};
