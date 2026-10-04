#include "PcaHardware.h"
#include "Display.h"
#include "Esp32Sensors.h"

struct SerialEvents : GameEvents {
  void message(const char *text) override { Serial.println(text); }
  void boardChanged(const Game::Board &board) override {
    Serial.println("  1 2 3 4 5 6 7");
    for (const auto &row : board.cells) {
      Serial.print("| ");
      for (char cell : row) { Serial.print(cell); Serial.print(' '); }
      Serial.println('|');
    }
  }
  void searchDone(const AI::SearchResult &search, uint32_t duration) override {
    Serial.print("Robot column "); Serial.print(search.column+1);
    Serial.print("; search "); Serial.print(duration); Serial.print(" ms; nodes "); Serial.println(search.nodes);
  }
  uint32_t now() override { return millis(); }
  uint32_t nowUs() override { return micros(); }
};

PcaHardware hardware;
HatchSequence hatches(hardware);
SerialEvents output;
Esp32Sensors sensorPort;
SensorService sensors(sensorPort);
Controller game(hatches,sensors,output);
CommandReader commands;
GameDisplay screen;
uint32_t lastBusCheck = 0, lastRevision = UINT32_MAX;

void readCommands() {
  // Snapshot the queued bytes so continuous input cannot starve sequencing.
  int available = Serial.available();
  while (available-- > 0) commands.feed(char(Serial.read()),game,millis(),output);
}

void checkBus() {
  const uint32_t now = millis();
  if (game.phase != Phase::Fault && uint32_t(now-lastBusCheck) >= Config::BUS_CHECK_MS) {
    lastBusCheck = now;
    if (!hardware.healthy()) game.fault();
  }
}

bool continueSearch() {
  readCommands(); checkBus(); game.pollSensors(); yield();
  return game.phase == Phase::RobotSearch;
}

void setup() {
  // External 10 kOhm OE pull-up is required while ESP32 is in reset.
  digitalWrite(Config::OE,HIGH); pinMode(Config::OE,OUTPUT);
  Serial.begin(Config::SERIAL_BAUD);
  screen.begin();
  sensorPort.begin();
  Serial.println("\nConnect Four bench: typed human moves, IR-confirmed robot, PCA7 indexer. Clear board/feed before starting.");
  for (uint8_t channel = 0; channel < 7; ++channel) if (!Config::HATCH_ENABLED[channel]) {
    Serial.print("PCA channel "); Serial.print(channel); Serial.println(" isolated: no servo motion.");
  }
  game.keepSearching = continueSearch;
  if (!hardware.begin()) game.fault();
  else game.restart();
  screen.render(game); lastRevision = game.revision;
}

void loop() {
  checkBus(); readCommands();
  // Draw thinking/turn status before entering the next search.
  if (lastRevision != game.revision) { screen.render(game); lastRevision = game.revision; }
  game.tick(millis());
  if (lastRevision != game.revision) { screen.render(game); lastRevision = game.revision; }
  yield();
}
