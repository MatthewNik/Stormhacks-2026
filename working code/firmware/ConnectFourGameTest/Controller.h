#pragma once
#include "AI.h"
#include "Hatch.h"
#include "Sensors.h"
#include <string.h>

enum class Mode { FreePlay, Coach };
enum class Phase { AwaitClear, ModeSelect, FirstPlayer, Difficulty, HumanOpening, HumanReady, ClosingForRobot,
  RobotSearch, RobotOpening, RobotBaseline, IndexerLoading, IndexerRelease,
  RobotConfirm, RobotQuiet, RobotClosing, EndClosing, Ended, Stopped, Fault,
  Paused, Correction, ManualBaseline, ManualWait, AwaitCorrection,
  StartupPositioning, HumanBaseline, HumanConfirm, IndexerReset };
struct GameEvents {
  virtual ~GameEvents() = default;
  virtual void message(const char *text) = 0;
  virtual void boardChanged(const Game::Board &board) = 0;
  virtual void searchDone(const AI::SearchResult &search, uint32_t duration) = 0;
  virtual uint32_t now() = 0;
  virtual uint32_t nowUs() { return now()*1000; }
};
class Controller : public SensorObserver {
  HatchSequence &outputs;
  SensorService &sensors;
  GameEvents &events;
  uint32_t phaseStarted = 0, releaseMs = 0, releaseUs = 0, generation = 0;
  bool manual = false, received = false, seenDetection = false;
  int humanColumn = -1;
  void prepareHuman(uint32_t now) {
    humanColumn = -1; received = seenDetection = false;
    sensors.reset(false); outputs.begin(0x7f,now); setPhase(Phase::HumanOpening);
  }
  void pause(const char *reason) {
    ++generation; outputs.disable(); sensors.reset(false);
    if (outputs.faulted) { fault(); return; }
    setPhase(Phase::Paused); events.message(reason);
    events.message("Paused. Check the machine, then press Centre (or type resume) to continue; restart to start over.");
  }
  bool humanPhase() const {
    return phase == Phase::HumanOpening || phase == Phase::HumanBaseline ||
           phase == Phase::HumanReady || phase == Phase::HumanConfirm;
  }
  void rebaselineHuman() {
    humanColumn = -1; received = seenDetection = false;
    sensors.reset(true); setPhase(Phase::HumanBaseline);
  }
  void closeAfterHuman(uint32_t now) {
    result = board.result();
    if (result == Game::Result::Playing) {
      sensors.reset(Config::ROBOT_IR_CONFIRM);
      if (Config::ROBOT_IR_CONFIRM && !sensors.clear()) { pause("Sensor active at the human-to-robot boundary"); return; }
      setPhase(Phase::RobotSearch);
    } else {
      outputs.begin(0,now);
      if (!outputs.command(7,Config::INDEXER.open,now)) { fault(); return; }
      setPhase(Phase::EndClosing);
    }
  }
  void commit(uint32_t now) {
    if (pendingColumn < 0) return;
    if (Config::ROBOT_IR_CONFIRM || manual) {
      if (!received || !sensors.seal(events.nowUs(),*this)) return;
    }
    if (phase != Phase::RobotClosing && phase != Phase::AwaitCorrection) return;
    const int committedColumn = pendingColumn;
    if (!board.drop(committedColumn,robot)) { pause("Pending column no longer legal"); return; }
    recordMove(committedColumn,robot);
    pendingColumn = -1; received = seenDetection = false;
    result = board.result(); events.boardChanged(board);
    if (!outputs.command(7,Config::INDEXER.open,now)) { fault(); return; }
    setPhase(Phase::IndexerReset);
  }
public:
  struct Move { uint8_t column; char symbol; };
  Move history[42] = {};
  uint8_t moveCount = 0;
  uint32_t bootId = 0, gameId = 0;
  Mode mode = Mode::FreePlay;
  Mode highlightedMode = Mode::FreePlay;
  bool robotFirst = false;
  Game::Board board;
  Game::Result result = Game::Result::Playing;
  Phase phase = Phase::AwaitClear;
  char human = 'O', robot = 'X';
  int difficulty = -1, pendingColumn = -1;
  uint32_t revision = 0;
  AI::KeepSearching keepSearching = nullptr;
  void recordMove(int column, char symbol) {
    if (moveCount < 42) history[moveCount++] = {uint8_t(column+1),symbol};
    ++revision;
  }
  void beginPlay(uint32_t now) {
    if (Config::MENU_ONLY) {
      highlightedMode = Mode::FreePlay;
      setPhase(Phase::ModeSelect);
      events.message("MENU TEST: selection confirmed; motors disabled. Returned to mode menu.");
      return;
    }
    human = robotFirst ? 'X' : 'O'; robot = Game::other(human);
    if (robotFirst) { sensors.reset(Config::ROBOT_IR_CONFIRM); setPhase(Phase::RobotSearch); }
    else prepareHuman(now);
  }
  void chooseMode(Mode selected, uint32_t) {
    if (phase != Phase::ModeSelect) return;
    if (selected == Mode::Coach) { events.message("Coach unavailable; select Free Play."); return; }
    mode = highlightedMode = selected; difficulty = 0; robotFirst = false;
    setPhase(Phase::Difficulty);
  }
  Controller(HatchSequence &scheduler, SensorService &service, GameEvents &output)
    : outputs(scheduler), sensors(service), events(output) {}
  void setPhase(Phase value) { phase = value; phaseStarted = events.now(); ++revision; }
  void fault() {
    if (phase == Phase::Fault) return;
    ++generation; outputs.fault(); sensors.reset(false); setPhase(Phase::Fault);
    events.message("ERROR: PCA fault. Outputs disabled; repair and reset ESP32.");
    events.message(outputs.error());
  }
  void restart() {
    if (phase == Phase::Fault || outputs.faulted) { fault(); return; }
    ++generation; outputs.disable(); sensors.reset(false);
    if (outputs.faulted) { fault(); return; }
    setPhase(Phase::AwaitClear);
    events.message("Clear board, indexer and feed path. Then type confirm-clear. Stored board retained until confirmation.");
  }
  void startup(uint32_t now) {
    if (phase == Phase::Fault || outputs.faulted) { fault(); return; }
    ++generation; outputs.disable(); sensors.reset(false);
    if (outputs.faulted) { fault(); return; }
    setPhase(Phase::AwaitClear);
    command("confirm-clear",now);
  }
  // Human turn: the first legal-column detection wins; later or stray triggers
  // are ignored rather than pausing play.
  void detected(uint8_t column, uint32_t startUs) override {
    if (phase == Phase::HumanReady) {
      if (!board.legal(column)) return;
      humanColumn = column; seenDetection = true; received = false;
      setPhase(Phase::HumanConfirm); return;
    }
    if (humanPhase()) return;
    if (phase == Phase::RobotBaseline || phase == Phase::ManualBaseline) {
      pause("Disc detected before robot/manual release was armed"); return;
    }
    const bool accepting = phase == Phase::IndexerRelease || phase == Phase::RobotConfirm || phase == Phase::ManualWait;
    if (!accepting || int32_t(startUs-releaseUs) < 0) { pause("Premature or extra sensor detection"); return; }
    if (column != pendingColumn) { pause("Wrong-column sensor detection"); return; }
    if (seenDetection) { pause("Extra robot chip detected"); return; }
    seenDetection = true;
  }
  void passage(uint8_t column, uint32_t startUs) override {
    if (phase == Phase::HumanConfirm) {
      if (column == humanColumn && seenDetection) received = true;
      return;
    }
    if (humanPhase()) return;
    if (phase == Phase::RobotBaseline || phase == Phase::ManualBaseline) return;
    if (!seenDetection || column != pendingColumn || int32_t(startUs-releaseUs) < 0 || received) {
      pause("Unexpected passage"); return;
    }
    received = true;
  }
  void sensorProblem(const char *reason) override {
    if (humanPhase() && strcmp(reason,"Sensor stuck active")) { rebaselineHuman(); return; }
    pause(reason);
  }
  // Centre button / "resume": operator confirms the machine is fine.
  void resume(uint32_t now) {
    if (phase != Phase::Paused) return;
    events.message("Resumed by operator.");
    if (pendingColumn < 0) { prepareHuman(now); return; }
    outputs.begin(0,now); setPhase(Phase::RobotClosing);
  }
  void pollSensors() { sensors.poll(events.nowUs(),*this); }
  void command(const char *line, uint32_t now) {
    if (!strcmp(line,"help")) {
      events.message("confirm-clear; free; coach unavailable; easy/medium/hard; 0 human or 1 robot. Human moves: IR only.");
      events.message("help, board, diagnose, stop, restart, resume (or Centre button when paused); recovery: correct, arm-manual, confirm-correction."); return;
    }
    if (!strcmp(line,"resume") || (!strcmp(line,"correct") && phase == Phase::Paused && pendingColumn < 0)) {
      if (phase == Phase::Paused) resume(now); else events.message("ERROR: resume only applies while paused.");
      return;
    }
    if (!strcmp(line,"board")) { events.boardChanged(board); return; }
    if (!strcmp(line,"restart")) { restart(); return; }
    if (!strcmp(line,"stop")) {
      if (phase != Phase::Fault) {
        ++generation; outputs.disable(); sensors.reset(false);
        if (outputs.faulted) { fault(); return; }
        setPhase(Phase::Stopped);
      }
      events.message("Outputs disabled; pending move retained. Use correct or restart."); return;
    }
    if (!strcmp(line,"confirm-clear") && phase == Phase::AwaitClear) {
      board.clear(); result = Game::Result::Playing; pendingColumn = -1; difficulty = -1;
      moveCount = 0; ++gameId; mode = highlightedMode = Mode::FreePlay; robotFirst = false;
      received = seenDetection = manual = false; humanColumn = -1; human = 'O'; robot = 'X';
      sensors.reset(false);
      if (Config::MENU_ONLY) setPhase(Phase::ModeSelect);
      else { outputs.beginStartup(now); setPhase(Phase::StartupPositioning); }
      events.boardChanged(board);
      events.message("Mode follows positioning: select Free Play. Coach unavailable."); return;
    }
    if (!strcmp(line,"correct") && (phase == Phase::Paused || phase == Phase::Stopped) && pendingColumn >= 0) {
      outputs.disable(); sensors.reset(false);
      if (outputs.faulted) { fault(); return; }
      setPhase(Phase::Correction);
      events.message("Secure indexer and correct the board/feed path. Then arm-manual; no magazine motion will occur."); return;
    }
    if (!strcmp(line,"arm-manual") && phase == Phase::Correction) {
      manual = true; received = seenDetection = false;
      sensors.reset(true); setPhase(Phase::ManualBaseline);
      events.message("Checking clear sensors; wait for the manual-delivery prompt."); return;
    }
    if (!strcmp(line,"confirm-correction") && phase == Phase::AwaitCorrection) {
      commit(now); return;
    }
    if (phase == Phase::ModeSelect) {
      if (!strcmp(line,"free")) chooseMode(Mode::FreePlay,now);
      else if (!strcmp(line,"coach")) chooseMode(Mode::Coach,now);
      else events.message("ERROR: select free; Coach unavailable.");
      return;
    }
    if (phase == Phase::FirstPlayer) {
      if (strcmp(line,"0") && strcmp(line,"1")) { events.message("ERROR: select exactly 0 or 1."); return; }
      robotFirst = *line == '1'; beginPlay(now); return;
    }
    if (phase == Phase::Difficulty) {
      if (!strcmp(line,"easy")) difficulty = 0;
      else if (!strcmp(line,"medium")) difficulty = 1;
      else if (!strcmp(line,"hard")) difficulty = 2;
      else { events.message("ERROR: select easy, medium, or hard."); return; }
      events.message(line); setPhase(Phase::FirstPlayer); return;
    }
    if (phase != Phase::HumanReady) { events.message("ERROR: command unavailable; use help."); return; }
    events.message("Human moves are sensor-only. Insert one disc when the display says ready.");
  }
  void tick(uint32_t now) {
    outputs.tick(now);
    if (outputs.faulted) { fault(); return; }
    pollSensors();
    const uint32_t elapsed = uint32_t(now-phaseStarted);
    switch (phase) {
      case Phase::StartupPositioning:
        if (!outputs.busy()) setPhase(Phase::ModeSelect);
        break;
      case Phase::HumanOpening:
        if (!outputs.busy()) { sensors.reset(true); setPhase(Phase::HumanBaseline); } break;
      case Phase::HumanBaseline:
        if (sensors.stableClear(events.nowUs())) { setPhase(Phase::HumanReady); events.message("Human turn: insert ONE disc; IR registers its column."); }
        break;
      case Phase::HumanConfirm:
        if (received && sensors.stableClear(events.nowUs())) {
          if (!sensors.seal(events.nowUs(),*this) || phase != Phase::HumanConfirm) break;
          if (!board.drop(humanColumn,human)) { pause("Human column no longer legal"); break; }
          recordMove(humanColumn,human); humanColumn = -1;
          events.boardChanged(board); closeAfterHuman(now);
        }
        break;
      case Phase::ClosingForRobot:
        if (!outputs.busy()) { sensors.reset(true); setPhase(Phase::RobotSearch); } break;
      case Phase::RobotSearch: {
        const uint32_t token = generation, started = events.now();
        auto choice = AI::choose(board,robot,Config::SEARCH_DEPTHS[difficulty],keepSearching);
        if (mode == Mode::Coach && moveCount == 1 && board.legal(3) && !choice.aborted) {
          bool tactical = false;
          for (int c = 0; c < 7; ++c) if (board.legal(c)) {
            for (int s = 0; s < 2; ++s) {
              const char symbol = s == 0 ? human : robot;
              Game::Board candidate = board; candidate.drop(c,symbol);
              if (candidate.result() != Game::Result::Playing) tactical = true;
            }
          }
          if (!tactical) choice.column = 3;
        }
        if (generation != token || phase != Phase::RobotSearch || choice.aborted) return;
        events.searchDone(choice,uint32_t(events.now()-started)); pendingColumn = choice.column;
        if (!board.legal(pendingColumn)) { pause("No legal robot column"); return; }
        manual = received = seenDetection = false;
        if (!Config::ROBOT_IR_CONFIRM) sensors.reset(false);
        if (!Config::HATCH_ENABLED[pendingColumn]) events.message("Selected hatch isolated: software/IR bench confirmation only.");
        outputs.begin(uint8_t(1 << pendingColumn),events.now());
        setPhase(Phase::RobotOpening); break;
      }
      case Phase::RobotOpening:
        if (!outputs.busy()) setPhase(Phase::RobotBaseline); break;
      case Phase::RobotBaseline: case Phase::ManualBaseline:
        if ((!Config::ROBOT_IR_CONFIRM && !manual) || sensors.stableClear(events.nowUs())) {
          received = seenDetection = false;
          if (manual) {
            releaseMs = now; releaseUs = events.nowUs(); setPhase(Phase::ManualWait);
            events.message("Manually deliver ONE robot chip through the pending target sensor now.");
          } else {
            if (!outputs.command(7,Config::INDEXER.open,now)) { fault(); return; }
            setPhase(Phase::IndexerLoading); events.message("Indexer 110: loading.");
          }
        } else if (elapsed >= Config::BASELINE_TIMEOUT_MS) pause("Sensors did not establish a clear baseline");
        break;
      case Phase::IndexerLoading:
        if (elapsed >= Config::INDEXER_SETTLE_MS+Config::INDEXER_LOAD_MS) {
          if (Config::ROBOT_IR_CONFIRM && !sensors.clear()) { pause("Sensor active before release"); return; }
          // The post-write timestamp is conservative: pre-command activity cannot count.
          if (!outputs.command(7,Config::INDEXER.closed,now)) { fault(); return; }
          releaseMs = events.now(); releaseUs = events.nowUs(); setPhase(Phase::IndexerRelease);
          events.message(Config::ROBOT_IR_CONFIRM ? "Indexer 180: release; waiting for target IR passage."
                                                  : "Indexer 180: release.");
        } break;
      case Phase::IndexerRelease:
        if (Config::ROBOT_IR_CONFIRM) { if (elapsed >= Config::INDEXER_SETTLE_MS) setPhase(Phase::RobotConfirm); }
        else if (elapsed >= Config::INDEXER_SETTLE_MS+Config::ROBOT_DROP_MS) {
          if (!outputs.command(7,Config::INDEXER.open,now)) { fault(); return; }
          events.message("Indexer 110: back to load.");
          outputs.begin(0,now); setPhase(Phase::RobotClosing);
        }
        break;
      case Phase::RobotConfirm: case Phase::ManualWait:
        if (received) setPhase(Phase::RobotQuiet);
        else if (uint32_t(now-releaseMs) >= Config::PASSAGE_TIMEOUT_MS) pause("Robot passage timeout; no automatic retry");
        break;
      case Phase::RobotQuiet:
        if (sensors.stableClear(events.nowUs())) { outputs.begin(0,now); setPhase(Phase::RobotClosing); }
        else if (elapsed >= Config::BASELINE_TIMEOUT_MS) pause("Sensors did not settle after passage");
        break;
      case Phase::RobotClosing:
        if (!outputs.busy()) {
          if (pendingColumn < 0) prepareHuman(now);
          else if (manual) { setPhase(Phase::AwaitCorrection); events.message("Compare physical board and pending move, then confirm-correction."); }
          else commit(now);
        }
        if (phase == Phase::RobotClosing && elapsed >= 6*Config::COMMAND_GAP_MS+Config::SETTLE_MS+Config::BASELINE_TIMEOUT_MS)
          pause("Sensors did not settle for commitment");
        break;
      case Phase::EndClosing:
        if (!outputs.busy()) {
          setPhase(Phase::Ended);
          events.message(result == Game::Result::Draw ? "Draw. Use restart." : result == Game::Result::OWins ? "O wins. Use restart." : "X wins. Use restart.");
        } break;
      case Phase::IndexerReset:
        if (elapsed >= Config::INDEXER_SETTLE_MS+Config::INDEXER_LOAD_MS) {
          if (result == Game::Result::Playing) prepareHuman(now);
          else {
            setPhase(Phase::Ended);
            events.message(result == Game::Result::Draw ? "Draw. Use restart." : result == Game::Result::OWins ? "O wins. Use restart." : "X wins. Use restart.");
          }
        }
        break;
      default: break;
    }
  }
};

class CommandReader {
  char buffer[48] = {};
  unsigned length = 0;
  bool invalid = false, moveBlocked = false;
public:
  void feed(char ch, Controller &game, uint32_t now, GameEvents &events) {
    if (ch == '\r' || ch == '\n') {
      if (invalid) events.message("ERROR: oversized or non-ASCII command ignored.");
      else {
        buffer[length] = 0;
        while (length && (buffer[length-1] == ' ' || buffer[length-1] == '\t')) buffer[--length] = 0;
        const char *line = buffer; while (*line == ' ' || *line == '\t') ++line;
        if (*line) {
          if (moveBlocked && game.phase == Phase::HumanReady && strlen(line) == 1 && *line >= '1' && *line <= '7')
            events.message("ERROR: move began before human hatches were ready.");
          else game.command(line,now);
        }
      }
      length = 0; invalid = false;
    } else {
      if (!length && !invalid) moveBlocked = game.phase != Phase::HumanReady;
      const uint8_t value = uint8_t(ch);
      if ((value < 32 && ch != '\t') || value >= 127 || length >= sizeof(buffer)-1) invalid = true;
      else if (!invalid) buffer[length++] = ch;
    }
  }
};
