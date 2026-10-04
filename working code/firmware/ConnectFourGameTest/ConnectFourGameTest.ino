#include "PcaHardware.h"
#include "Display.h"
#include "Esp32Sensors.h"
#include "Buttons.h"
#include "Protocol.h"
#include <esp_system.h>

struct SerialEvents : GameEvents, ProtocolReply {
  FrameQueue tx;
  bool requested = true;
  void message(const char *text) override {
    JsonFrame f; f.add("{\"v\":1,\"type\":\"message\",\"text\":"); f.quoted(text); f.add("}\n"); tx.push(f);
  }
  void boardChanged(const Game::Board &) override { requested = true; }
  void searchDone(const AI::SearchResult &s, uint32_t duration) override {
    JsonFrame f; f.add("{\"v\":1,\"type\":\"search\",\"column\":%d,\"duration_ms\":%lu,\"nodes\":%lu}\n",
      s.column+1,(unsigned long)duration,(unsigned long)s.nodes); tx.push(f);
  }
  void snapshot() override { requested = true; }
  void commandResult(uint32_t request, const char *status) override {
    JsonFrame f; f.add("{\"v\":1,\"type\":\"command_result\",\"request_id\":%lu,\"status\":\"%s\"}\n",
      (unsigned long)request,status); tx.push(f); requested = true;
  }
  uint32_t now() override { return millis(); }
  uint32_t nowUs() override { return micros(); }
};
// Menu-test builds never construct or invoke the motor driver.
#if MENU_TEST_ONLY
struct MenuTestHardware : HatchIO {
  bool writeAngle(uint8_t, uint16_t) override { return true; }
  bool stopChannel(uint8_t) override { return true; }
  void enable(bool) override {}
  bool begin() { return true; }
  bool healthy() { return true; }
} hardware;
#else
PcaHardware hardware;
#endif
HatchSequence hatches(hardware);
SerialEvents output;
Esp32Sensors sensorPort;
SensorService sensors(sensorPort);
Controller game(hatches,sensors,output);
ProtocolReader commands;
MenuButtons buttons;
GameDisplay screen;
uint32_t lastBusCheck = 0, lastRevision = UINT32_MAX, sentRevision = UINT32_MAX, sentDropped = 0;
uint32_t sentGame = UINT32_MAX;
uint8_t sentMoves = 0;

void pumpSerial() {
  unsigned remaining = 0; const char *data = output.tx.front(remaining);
  int available = Serial.availableForWrite();
  if (data && available > 0) {
    unsigned bytes = remaining < 32 ? remaining : 32;
    if (bytes > unsigned(available)) bytes = unsigned(available);
    output.tx.consume(unsigned(Serial.write(reinterpret_cast<const uint8_t *>(data),bytes)));
  }
  if (!output.tx.room()) return;
  const bool moved = sentGame == game.gameId && sentMoves != game.moveCount;
  if (output.requested || sentRevision != game.revision || sentDropped != output.tx.dropped) {
    const char *type = sentDropped != output.tx.dropped ? "snapshot" : moved ? "move" : output.requested ? "snapshot" : "state";
    if (output.tx.push(stateFrame(game,type,output.tx.dropped))) {
      output.requested = false; sentRevision = game.revision; sentDropped = output.tx.dropped;
      sentMoves = game.moveCount; sentGame = game.gameId;
    }
  }
}
void readCommands() {
  int available = Serial.available(); if (available > 64) available = 64;
  while (available-- > 0) commands.feed(char(Serial.read()),game,millis(),output,output);
}
void checkBus() {
  const uint32_t now = millis();
  if (game.phase != Phase::Fault && uint32_t(now-lastBusCheck) >= Config::BUS_CHECK_MS) {
    lastBusCheck = now; if (!hardware.healthy()) game.fault();
  }
}
void pollButtons() {
  buttons.poll(digitalRead(Config::BUTTON_LEFT) == LOW,digitalRead(Config::BUTTON_RIGHT) == LOW,
    digitalRead(Config::BUTTON_CENTRE) == LOW,game,millis());
}
bool continueSearch() {
  readCommands(); checkBus(); game.pollSensors(); pumpSerial(); yield();
  return game.phase == Phase::RobotSearch;
}
void setup() {
  digitalWrite(Config::OE,HIGH); pinMode(Config::OE,OUTPUT);
  pinMode(Config::BUTTON_LEFT,INPUT_PULLUP); pinMode(Config::BUTTON_RIGHT,INPUT_PULLUP);
  pinMode(Config::BUTTON_CENTRE,INPUT_PULLUP);
  game.bootId = esp_random();
  Serial.begin(Config::SERIAL_BAUD);
  screen.begin(); if (!Config::MENU_ONLY) sensorPort.begin(); game.keepSearching = continueSearch;
  if (!hardware.begin()) game.fault(); else game.restart();
  if (Config::MENU_ONLY) {
    game.command("confirm-clear",millis());
    output.message("MENU TEST ONLY: PCA/sensors disabled; gameplay unavailable.");
  }
  screen.render(game); lastRevision = game.revision;
}
void loop() {
  checkBus(); readCommands(); pollButtons();
  if (lastRevision != game.revision) { screen.render(game); lastRevision = game.revision; }
  game.tick(millis());
  if (lastRevision != game.revision) { screen.render(game); lastRevision = game.revision; }
  pumpSerial(); yield();
}
