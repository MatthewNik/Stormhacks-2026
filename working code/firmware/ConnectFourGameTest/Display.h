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
      if (game.highlightedMode == Mode::FreePlay) menu(Config::MENU_ONLY ? "TEST: Mode" : "Mode","FREE","PLAY");
      else {
        text(0,0,"Mode",CYAN); oled.setTextSize(2); text(0,14,"COACH",YELLOW);
        oled.setTextSize(1); text(0,36,"Unavailable"); text(0,56,"< Left/Right >");
      }
      return;
    }
    if (game.phase == Phase::FirstPlayer) {
      menu("Who starts?",game.robotFirst ? "ROBOT" : "HUMAN"); return;
    }
    if (game.phase == Phase::Difficulty) {
      menu("Difficulty",game.difficulty == 0 ? "EASY" : game.difficulty == 1 ? "MEDIUM" : "HARD"); return;
    }
    if (game.phase == Phase::StartupPositioning) {
      text(0,0,"Positioning",CYAN); text(0,16,"Doors: up/open");
      text(0,30,"Motor 7: load"); text(0,48,"Please wait"); return;
    }
    text(0,0,game.phase == Phase::Fault ? "PCA Fault" : "Free Play",CYAN);
    text(0,12,game.difficulty == 0 ? "Easy" : game.difficulty == 1 ? "Medium" : game.difficulty == 2 ? "Hard" : "",WHITE);
    const char *status = "Hatches moving";
    switch (game.phase) {
      case Phase::HumanReady: status = "Insert ONE disc"; break;
      case Phase::HumanBaseline: status = "IR: clear check"; break;
      case Phase::HumanConfirm: status = "Human: verify IR"; break;
      case Phase::IndexerReset: status = "Indexer: reload"; break;
      case Phase::HumanOpening: status = "Human: opening"; break;
      case Phase::ClosingForRobot: status = "Robot: closing"; break;
      case Phase::RobotSearch: status = "Robot: thinking"; break;
      case Phase::RobotOpening: status = "Robot: opening"; break;
      case Phase::RobotBaseline: status = "IR: clear check"; break;
      case Phase::IndexerLoading: status = "Indexer: load 110"; break;
      case Phase::IndexerRelease: status = "Indexer: drop 180"; break;
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
    text(0,28,status,YELLOW);
    if (game.phase == Phase::Fault) text(0,46,"Type diagnose");
    else if (game.phase == Phase::HumanReady) text(0,46,"IR input active");
    else if (game.pendingColumn >= 0) {
      char target[] = "Robot column: 1"; target[14] = char('1'+game.pendingColumn);
      text(0,46,target);
    }
  }
};
