#include "../Controller.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <cstring>
#include <deque>

struct Write { uint8_t channel; uint16_t angle; uint32_t time; };
struct FakeIO : HatchIO {
  std::vector<Write> writes;
  bool enabled = false, fail = false;
  bool pulses[8] = {};
  std::vector<uint32_t> stoppedAt;
  uint32_t time = 0;
  void enable(bool value) override { enabled = value; }
  bool writeAngle(uint8_t channel, uint16_t angle) override {
    if (fail) return false;
    pulses[channel] = channel == 7 || Config::HATCH_ENABLED[channel];
    writes.push_back({channel,angle,time}); return true;
  }
  bool stopChannel(uint8_t channel) override {
    if (fail) return false;
    pulses[channel] = false; stoppedAt.push_back(time); return true;
  }
};
struct Events : GameEvents {
  std::vector<std::string> messages;
  unsigned boards = 0, searches = 0;
  uint32_t time = 0;
  AI::SearchResult search;
  void message(const char *text) override { messages.emplace_back(text); }
  void boardChanged(const Game::Board &) override { ++boards; }
  void searchDone(const AI::SearchResult &s, uint32_t) override { search = s; ++searches; }
  uint32_t now() override { return time; }
};
struct FakeSensors : SensorPort {
  bool levels[7] = {}, capturing = false, overflow = false;
  uint32_t timeUs = 0;
  std::deque<SensorEdge> queue;
  uint32_t reset(bool enabled, bool snapshot[7]) override {
    queue.clear(); overflow = false; capturing = enabled;
    for (int c = 0; c < 7; ++c) snapshot[c] = enabled && levels[c];
    return timeUs;
  }
  void edge(uint8_t column, bool active, uint32_t at) {
    levels[column] = active;
    if (!capturing) return;
    if (queue.size() >= Config::EDGE_QUEUE_SIZE) { overflow = true; return; }
    queue.push_back({column,active,at});
  }
  bool pop(SensorEdge &edge) override {
    if (queue.empty()) return false;
    edge = queue.front(); queue.pop_front(); return true;
  }
  bool overflowed() override { return overflow; }
  bool sealClear() override {
    if (overflow || !queue.empty()) return false;
    for (bool active : levels) if (active) return false;
    capturing = false; return true;
  }
};
struct Fixture {
  FakeIO io;
  Events events;
  HatchSequence hatches{io};
  FakeSensors port;
  SensorService sensors{port};
  Controller game{hatches,sensors,events};
  CommandReader reader;
  uint32_t time = 0;
  void clock() { io.time = events.time = time; port.timeUs = time*1000; }
  void tick() { clock(); game.tick(time); ++time; }
  void advance(unsigned ms) { while (ms--) tick(); }
  void edge(uint8_t c, bool active) { clock(); port.edge(c,active,time*1000); }
  void pulse(uint8_t c, unsigned ms = 5) { edge(c,true); advance(ms); edge(c,false); advance(21); }
  void until(Phase phase) {
    for (int i = 0; i < 20000 && game.phase != phase; ++i) tick();
    assert(game.phase == phase);
  }
  void line(const std::string &text) { clock(); for (char c : text) reader.feed(c,game,time,events); }
  void start(const char *first = "0", const char *level = "easy") {
    clock(); game.restart(); game.command("confirm-clear",time); until(Phase::ModeSelect); game.command("free",time); game.command(level,time); game.command(first,time);
  }
  void humanStart(const char *level = "easy") { start("0",level); until(Phase::HumanReady); }
};
static int discs(const Game::Board &board) {
  int count = 0; for (const auto &row : board.cells) for (char v : row) count += v != '.'; return count;
}
static bool equal(const Game::Board &a, const Game::Board &b) { return std::memcmp(a.cells,b.cells,sizeof(a.cells)) == 0; }
static Game::Board drawBoard() {
  Game::Board board;
  for (int r = 0; r < 6; ++r) std::memcpy(board.cells[r], r%2 ? "XXOOXXO" : "OOXXOOX",7);
  return board;
}
static void testBoard() {
  Game::Board b;
  assert(!b.drop(-1,'O') && !b.drop(7,'X') && !b.drop(0,'A'));
  for (int i = 0; i < 6; ++i) { assert(b.drop(0,i%2 ? 'X' : 'O')); assert(b.cells[5-i][0] == (i%2 ? 'X' : 'O')); }
  const auto before = b;
  assert(!b.drop(0,'O') && equal(b,before) && !b.legal(0));
  const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
  for (char symbol : {'O','X'}) for (const auto &d : dirs) {
    Game::Board win;
    for (int i = 0; i < 3; ++i) win.cells[i*d[0]][3+i*d[1]] = symbol;
    assert(win.result() == Game::Result::Playing);
    win.cells[3*d[0]][3+3*d[1]] = symbol;
    assert(win.result() == (symbol == 'O' ? Game::Result::OWins : Game::Result::XWins));
  }
  assert(drawBoard().result() == Game::Result::Draw);
}
static bool cancelSearch() { return false; }
static void testAI() {
  for (char robot : {'O','X'}) for (int depth : Config::SEARCH_DEPTHS) {
    Game::Board win, block;
    for (int c = 0; c < 3; ++c) { win.drop(c,robot); block.drop(c,Game::other(robot)); }
    auto before = win;
    auto choice = AI::choose(win,robot,depth);
    assert(choice.column == 3 && choice.score > 900000 && choice.nodes > 0 && equal(win,before));
    before = block; choice = AI::choose(block,robot,depth);
    assert(choice.column == 3 && equal(block,before));
    Game::Board opening; assert(AI::choose(opening,robot,depth).column == 3);
    Game::Board crowded = drawBoard(); crowded.cells[0][6] = '.';
    before = crowded; assert(AI::choose(crowded,robot,depth).column == 6 && equal(crowded,before));
  }
  Game::Board board;
  const auto before = board;
  const auto canceled = AI::choose(board,'X',5,cancelSearch);
  assert(canceled.aborted && canceled.column == -1 && equal(board,before));
  assert(AI::choose(drawBoard(),'O',5).column == -1);
}
static unsigned indexWrites(const Fixture &f, int angle = -1) {
  unsigned count = 0;
  for (const auto &w : f.io.writes) if (w.channel == 7 && (angle < 0 || w.angle == angle)) ++count;
  return count;
}
static void toRelease(Fixture &f) {
  f.humanStart(); f.pulse(3); f.until(Phase::IndexerRelease);
}
static void finishRobot(Fixture &f) {
  const auto before = f.game.board;
  const int target = f.game.pendingColumn;
  assert(target >= 0);
  f.pulse(uint8_t(target));
  assert(equal(before,f.game.board));
  f.until(Phase::HumanReady);
  assert(discs(f.game.board) == discs(before)+1 && f.game.board.cells[5][target] != '.');
}

inline void selectMenu(Fixture &f) {
  f.game.startup(f.time); f.until(Phase::ModeSelect);
}
static void humanMove(Fixture &f, uint8_t column) {
  f.pulse(column); f.until(Phase::RobotSearch);
}
static void testAutomaticStartup() {
  Fixture f; f.game.startup(0);
  assert(f.game.phase == Phase::StartupPositioning && !f.port.capturing);
  f.line("free\neasy\n1\n"); assert(f.game.phase == Phase::StartupPositioning);
  f.until(Phase::ModeSelect);
  assert(f.game.gameId == 1 && f.game.moveCount == 0 && f.io.writes.size() == 8);
  for (int c = 0; c < 8; ++c) {
    assert(f.io.writes[c].channel == c && f.io.writes[c].time == uint32_t(c*50));
    assert(f.io.writes[c].angle == (c == 7 ? 110 : Config::HATCHES[c].open));
    assert(f.io.pulses[c]);
  }
  assert(f.time >= 650 && f.io.enabled);
  const auto count = f.io.writes.size();
  f.advance(5000); assert(f.io.writes.size() == count && f.io.enabled);
  f.line("coach\n"); assert(f.game.phase == Phase::ModeSelect && f.game.mode == Mode::FreePlay);
  f.game.restart(); assert(f.game.phase == Phase::AwaitClear && !f.io.enabled);
  f.advance(1000); assert(f.io.writes.size() == count);
  f.line("confirm-clear\n"); f.until(Phase::ModeSelect); assert(f.game.gameId == 2);
  Fixture bad; bad.io.fail = true; bad.game.startup(0);
  assert(bad.game.phase == Phase::Fault && !bad.io.enabled);
}
static void testScheduler() {
  FakeIO io; HatchSequence seq(io); seq.beginStartup(0);
  for (uint32_t t = 0; t <= 650; ++t) { io.time = t; seq.tick(t); }
  assert(!seq.busy() && io.writes.size() == 8 && io.enabled);
  seq.tick(100000); for (bool pulse : io.pulses) assert(pulse);
  seq.begin(1 << 4,100000);
  for (uint32_t dt = 0; dt <= 600; ++dt) { io.time = 100000+dt; seq.tick(io.time); }
  for (int c = 0; c < 7; ++c) assert(io.writes[8+c].angle == (c == 4 ? Config::HATCHES[c].open : Config::HATCHES[c].closed));
  assert(io.pulses[7]); seq.disable(); assert(!io.enabled);
  for (bool pulse : io.pulses) assert(!pulse);
  seq.begin(0,UINT32_MAX-100);
  for (uint32_t dt = 0; dt <= 600; ++dt) seq.tick(uint32_t(UINT32_MAX-100+dt));
  assert(!seq.busy() && io.enabled);
  io.fail = true; assert(!seq.command(7,110,4000) && seq.faulted && !io.enabled);
}
static void testHumanSensors() {
  for (uint8_t c = 0; c < 7; ++c) {
    Fixture f; f.humanStart();
    assert(f.sensors.enabled && f.port.capturing && f.io.enabled);
    f.advance(10000); assert(f.game.phase == Phase::HumanReady && f.game.moveCount == 0);
    const auto before = f.game.board;
    f.line("1\n2\n7\n01\n"); assert(equal(before,f.game.board));
    f.pulse(c,1); f.advance(100); assert(f.game.phase == Phase::HumanReady && f.game.moveCount == 0);
    f.edge(c,true); f.advance(5); assert(f.game.phase == Phase::HumanConfirm && f.game.moveCount == 0);
    f.edge(c,false); f.advance(21); assert(f.game.moveCount == 0);
    f.until(Phase::RobotSearch);
    assert(f.game.moveCount == 1 && f.game.history[0].column == c+1 && f.game.history[0].symbol == 'O');
  }
  for (int failure = 0; failure < 5; ++failure) {
    Fixture f; f.humanStart(); const auto before = f.game.board;
    if (failure == 0) { f.pulse(0); f.pulse(0); }
    if (failure == 1) { f.edge(0,true); f.edge(1,true); f.advance(5); }
    if (failure == 2) { f.edge(0,true); f.advance(1001); }
    if (failure == 3) { for (unsigned i = 0; i <= Config::EDGE_QUEUE_SIZE; ++i) f.port.edge(0,i%2==0,f.time*1000+i); f.tick(); }
    if (failure == 4) { f.pulse(0); for (int i = 0; i < 100 && f.game.phase != Phase::Paused; ++i) { f.pulse(1,1); f.advance(20); } }
    assert(f.game.phase == Phase::Paused && equal(before,f.game.board) && !f.io.enabled && !f.sensors.enabled);
  }
  Fixture full; full.humanStart(); for (int i = 0; i < 6; ++i) full.game.board.drop(0,i%2 ? 'X' : 'O');
  const auto before = full.game.board; full.pulse(0);
  assert(full.game.phase == Phase::Paused && equal(before,full.game.board));
  Fixture baseline; baseline.start(); baseline.until(Phase::HumanOpening); baseline.port.levels[0] = true;
  baseline.until(Phase::Paused); assert(baseline.game.moveCount == 0 && !baseline.io.enabled);
}
static void testSequencing() {
  for (const char *level : {"easy","medium","hard"}) for (const char *first : {"0","1"}) {
    Fixture f; f.start(first,level);
    if (*first == '0') { f.until(Phase::HumanReady); humanMove(f,3); }
    const auto before = f.game.board; const auto offset = f.io.writes.size();
    f.until(Phase::IndexerLoading); const int target = f.game.pendingColumn;
    assert(target >= 0 && equal(before,f.game.board));
    for (int c = 0; c < 7; ++c) {
      const auto &w = f.io.writes[offset+c]; assert(w.channel == c);
      assert(w.angle == (c == target ? Config::HATCHES[c].open : Config::HATCHES[c].closed));
      if (c) assert(w.time-f.io.writes[offset+c-1].time >= 50);
    }
    const auto loads = indexWrites(f,110); f.until(Phase::IndexerRelease);
    assert(indexWrites(f,180) == 1 && f.io.writes.back().time-f.io.writes[f.io.writes.size()-2].time >= 800);
    finishRobot(f);
    assert(indexWrites(f,110) == loads+1 && indexWrites(f,180) == 1 && f.io.enabled);
    assert(f.game.history[f.game.moveCount-1].symbol == f.game.robot);
    for (int c = 0; c < 7; ++c) assert(f.io.writes[f.io.writes.size()-7+c].angle == Config::HATCHES[c].open);
    f.advance(5000); assert(f.game.phase == Phase::HumanReady && indexWrites(f,180) == 1);
    humanMove(f,0); f.until(Phase::IndexerRelease); assert(indexWrites(f,180) == 2);
  }
}
static void testSensorFailures() {
  for (int mode = 0; mode < 7; ++mode) {
    Fixture f; toRelease(f); const auto before = f.game.board; const int target = f.game.pendingColumn;
    if (mode == 0) f.pulse(uint8_t((target+1)%7));
    if (mode == 1) { f.pulse(uint8_t(target)); f.pulse(uint8_t(target)); }
    if (mode == 2) f.advance(3010);
    if (mode == 3) { f.edge(uint8_t(target),true); f.advance(1001); }
    if (mode == 4) { for (unsigned i = 0; i <= Config::EDGE_QUEUE_SIZE; ++i) f.port.edge(0,i%2==0,f.time*1000+i); f.tick(); }
    if (mode == 5) { f.pulse(uint8_t(target)); f.until(Phase::RobotClosing); f.pulse(uint8_t(target)); }
    if (mode == 6) { f.pulse(uint8_t(target)); f.until(Phase::RobotQuiet); for (int i = 0; i < 50 && f.game.phase != Phase::Paused; ++i) { f.pulse(0,1); f.advance(30); } }
    assert(f.game.phase == Phase::Paused && equal(before,f.game.board) && !f.io.enabled && indexWrites(f,180) == 1);
    const auto writes = f.io.writes.size(); f.advance(4000); assert(f.io.writes.size() == writes);
  }
  Fixture early; early.humanStart(); humanMove(early,3); early.until(Phase::IndexerLoading);
  const auto before = early.game.board; early.pulse(uint8_t(early.game.pendingColumn));
  assert(early.game.phase == Phase::Paused && equal(before,early.game.board) && indexWrites(early,180) == 0);
  Fixture crossing; crossing.humanStart(); humanMove(crossing,3); crossing.until(Phase::IndexerLoading);
  crossing.edge(uint8_t(crossing.game.pendingColumn),true); crossing.advance(1);
  crossing.until(Phase::Paused); assert(indexWrites(crossing,180) == 0);
  Fixture baseline; baseline.start("1"); baseline.until(Phase::RobotBaseline);
  baseline.pulse(0); assert(baseline.game.phase == Phase::Paused && indexWrites(baseline,180) == 0);
}
struct Observer : SensorObserver {
  unsigned detects = 0, passages = 0, errors = 0; uint32_t start = 0;
  void detected(uint8_t, uint32_t at) override { ++detects; start = at; }
  void passage(uint8_t, uint32_t at) override { ++passages; assert(at == start); }
  void sensorProblem(const char *) override { ++errors; }
};
static void testQualification() {
  FakeSensors port; SensorService service(port); Observer observer; service.reset(true);
  port.edge(0,true,1000); port.edge(0,false,1500); service.poll(22000,observer);
  assert(observer.detects == 0 && observer.passages == 0);
  port.edge(0,true,30000); port.edge(0,false,35000); port.edge(0,true,40000); port.edge(0,false,45000);
  service.poll(65000,observer); assert(observer.detects == 1 && observer.passages == 1 && observer.start == 30000);
  port.edge(1,true,100001); port.edge(1,false,100500);
  assert(!service.seal(100000,observer) && service.enabled);
  assert(!service.seal(200499,observer)); assert(service.seal(200500,observer));
  FakeSensors wrapped; wrapped.timeUs = UINT32_MAX-10000;
  SensorService wrap(wrapped); Observer seen; wrap.reset(true);
  wrapped.edge(0,true,UINT32_MAX-5000); wrapped.edge(0,false,1000);
  wrap.poll(21000,seen); assert(seen.detects == 1 && seen.passages == 1 && seen.errors == 0);
  // An edge racing the final seal prevents commitment and is processed next tick.
  struct Racing : FakeSensors {
    bool race = true;
    bool sealClear() override { if (race) { race = false; edge(1,true,timeUs); return false; } return FakeSensors::sealClear(); }
  } racing;
  FakeIO io; HatchSequence hatches(io); Events events; SensorService sensors(racing); Controller game(hatches,sensors,events);
  game.phase = Phase::HumanReady; sensors.reset(true);
  racing.edge(0,true,1000); racing.edge(0,false,6000); events.time = 30; racing.timeUs = 30000; game.tick(30);
  events.time = 110; racing.timeUs = 110000; game.tick(110); assert(game.moveCount == 0);
  events.time = 113; racing.timeUs = 113000; game.tick(113); assert(game.phase == Phase::Paused && game.moveCount == 0);
  // Activity arriving just after a successful seal cannot hide as a new epoch's initial level.
  struct AfterSeal : FakeSensors {
    bool sealClear() override {
      if (!FakeSensors::sealClear()) return false;
      levels[1] = true; return true;
    }
  } arriving;
  FakeIO otherIO; HatchSequence otherHatches(otherIO); Events otherEvents;
  SensorService otherSensors(arriving); Controller otherGame(otherHatches,otherSensors,otherEvents);
  otherGame.phase = Phase::HumanReady; otherSensors.reset(true);
  arriving.edge(0,true,1000); arriving.edge(0,false,6000); otherEvents.time = 110; arriving.timeUs = 110000;
  otherGame.tick(110);
  assert(otherGame.moveCount == 1 && otherGame.phase == Phase::Paused && !otherIO.enabled);
}
static void testRecoveryAndInterruptions() {
  for (Phase phase : {Phase::StartupPositioning,Phase::HumanOpening,Phase::HumanBaseline,Phase::HumanReady,
       Phase::HumanConfirm,Phase::RobotSearch,Phase::RobotOpening,Phase::RobotBaseline,Phase::IndexerLoading,
       Phase::IndexerRelease,Phase::RobotConfirm,Phase::RobotQuiet,Phase::RobotClosing,Phase::IndexerReset}) {
    for (int action : {0,1,2}) {
      Fixture f;
      if (phase == Phase::StartupPositioning) f.game.startup(0);
      else {
        f.start();
        if (phase == Phase::HumanOpening || phase == Phase::HumanBaseline || phase == Phase::HumanReady) f.until(phase);
        else {
          f.until(Phase::HumanReady); f.pulse(3);
          if (phase != Phase::HumanConfirm) {
            if (phase == Phase::RobotQuiet || phase == Phase::RobotClosing || phase == Phase::IndexerReset) {
              f.until(Phase::IndexerRelease); f.pulse(uint8_t(f.game.pendingColumn));
            }
            f.until(phase);
          }
        }
      }
      const auto before = f.game.board; const auto writes = f.io.writes.size();
      if (action == 2) f.game.fault(); else f.line(action == 1 ? "restart\n" : "stop\n"); f.advance(4000);
      assert(f.game.phase == (action == 2 ? Phase::Fault : action == 1 ? Phase::AwaitClear : Phase::Stopped));
      assert(equal(before,f.game.board) && f.io.writes.size() == writes && !f.io.enabled && !f.sensors.enabled);
    }
  }
  Fixture recovery; toRelease(recovery); recovery.advance(3010);
  const auto before = recovery.game.board; const auto releases = indexWrites(recovery,180);
  const int target = recovery.game.pendingColumn;
  recovery.line("correct\narm-manual\n"); recovery.until(Phase::ManualWait);
  recovery.pulse(uint8_t(target)); recovery.until(Phase::AwaitCorrection);
  assert(equal(before,recovery.game.board) && indexWrites(recovery,180) == releases);
  recovery.line("confirm-correction\n"); recovery.until(Phase::HumanReady);
  assert(discs(recovery.game.board) == discs(before)+1 && indexWrites(recovery,180) == releases);
  recovery.line("confirm-correction\n"); assert(discs(recovery.game.board) == discs(before)+1);
  for (Phase phase : {Phase::Correction,Phase::ManualBaseline,Phase::ManualWait,Phase::AwaitCorrection}) {
    Fixture cancel; toRelease(cancel); cancel.advance(3010); cancel.line("correct\n");
    if (phase != Phase::Correction) cancel.line("arm-manual\n");
    if (phase == Phase::ManualWait || phase == Phase::AwaitCorrection) cancel.until(Phase::ManualWait);
    if (phase == Phase::AwaitCorrection) { cancel.pulse(uint8_t(cancel.game.pendingColumn)); cancel.until(phase); }
    const auto stored = cancel.game.board; const auto writes = cancel.io.writes.size();
    cancel.line("restart\n"); cancel.advance(4000);
    assert(cancel.game.phase == Phase::AwaitClear && equal(stored,cancel.game.board) && cancel.io.writes.size() == writes && !cancel.sensors.enabled);
  }
  Fixture extra; toRelease(extra); extra.advance(3010); extra.line("correct\narm-manual\n"); extra.until(Phase::ManualWait);
  extra.pulse(uint8_t(extra.game.pendingColumn)); extra.until(Phase::AwaitCorrection); extra.pulse(uint8_t(extra.game.pendingColumn));
  assert(extra.game.phase == Phase::Paused && extra.game.moveCount == 1 && indexWrites(extra,180) == 1);
  Fixture fault; toRelease(fault); fault.io.fail = true; fault.game.fault();
  fault.line("restart\nconfirm-clear\ncorrect\narm-manual\n"); assert(fault.game.phase == Phase::Fault && !fault.io.enabled);
}
static Fixture *interruptFixture = nullptr;
static bool stopSearch() { interruptFixture->game.command("stop",interruptFixture->time); return false; }
static bool restartSearch() { interruptFixture->game.restart(); return true; }
static void testSearchCancellation() {
  for (auto hook : {stopSearch,restartSearch}) {
    Fixture f; f.humanStart("hard"); humanMove(f,3);
    interruptFixture = &f; f.game.keepSearching = hook; f.tick();
    assert(f.game.phase == (hook == stopSearch ? Phase::Stopped : Phase::AwaitClear) && indexWrites(f,180) == 0 && !f.io.enabled);
  }
}
static void testEndings() {
  Fixture win; win.humanStart(); for (int c = 0; c < 3; ++c) win.game.board.drop(c,'O');
  win.pulse(3); win.until(Phase::Ended); const auto final = win.game.board;
  win.advance(4000); win.line("5\n"); assert(equal(final,win.game.board) && win.events.searches == 0 && indexWrites(win,180) == 0);
  for (int c = 0; c < 7; ++c) assert(win.io.writes[win.io.writes.size()-7+c].angle == Config::HATCHES[c].closed);
  Fixture robot; robot.start("1"); for (int c = 0; c < 3; ++c) robot.game.board.drop(c,'O');
  robot.until(Phase::IndexerRelease); robot.pulse(uint8_t(robot.game.pendingColumn)); robot.until(Phase::Ended);
  assert(robot.game.result == Game::Result::OWins && discs(robot.game.board) == 4 && robot.io.writes.back().angle == 110);
  Fixture draw; draw.humanStart(); draw.game.board = drawBoard(); draw.game.board.cells[0][0] = '.';
  draw.pulse(0); draw.until(Phase::Ended); assert(draw.game.result == Game::Result::Draw && indexWrites(draw,180) == 0);
}
static void testCompleteGames() {
  for (const char *level : {"easy","medium","hard"}) for (const char *first : {"0","1"}) {
    Fixture f; f.start(first,level);
    for (int ticks = 0; ticks < 200000 && f.game.phase != Phase::Ended; ++ticks) {
      if (f.game.phase == Phase::HumanReady) {
        int column = 0; while (column < 7 && !f.game.board.legal(column)) ++column;
        assert(column < 7); f.pulse(uint8_t(column));
      } else if (f.game.phase == Phase::IndexerRelease) {
        f.pulse(uint8_t(f.game.pendingColumn));
        f.until(Phase::RobotQuiet);
      } else f.tick();
      assert(f.game.phase != Phase::Paused && f.game.phase != Phase::Fault);
    }
    assert(f.game.phase == Phase::Ended && f.game.moveCount <= 42 && f.game.moveCount == discs(f.game.board));
    Game::Board replay; unsigned robotMoves = 0;
    for (unsigned i = 0; i < f.game.moveCount; ++i) {
      const auto &move = f.game.history[i]; assert(move.symbol == (i%2 ? 'X' : 'O'));
      assert(replay.drop(move.column-1,move.symbol)); robotMoves += move.symbol == f.game.robot;
      if (i+1 < f.game.moveCount) assert(replay.result() == Game::Result::Playing);
    }
    assert(equal(replay,f.game.board) && indexWrites(f,180) == robotMoves);
    assert(!f.sensors.enabled && f.io.enabled);
  }
}
int main() {
  testAutomaticStartup(); testBoard(); testAI(); testScheduler(); testHumanSensors(); testSequencing();
  testQualification(); testSensorFailures(); testRecoveryAndInterruptions(); testSearchCancellation(); testEndings(); testCompleteGames();
  puts("PASS: startup/holding, game/AI, human and robot IR, target doors, faults, recovery, atomic commitment and endings.");
  return 0;
}
