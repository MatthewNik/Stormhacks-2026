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
    clock(); game.restart(); game.command("confirm-clear",time); game.command("free",time); game.command(level,time); game.command(first,time);
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
  f.humanStart(); f.line("4\n"); f.until(Phase::IndexerRelease);
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
static void testMenusAndInput() {
  Fixture f; f.game.restart(); assert(f.game.phase == Phase::AwaitClear && !f.io.enabled);
  f.line("0\neasy\n"); assert(f.game.phase == Phase::AwaitClear);
  f.line("confirm-clear\n2\n0 extra\n"); assert(f.game.phase == Phase::ModeSelect);
  f.line(" free \r\n"); assert(f.game.human == 'O' && f.game.robot == 'X' && !f.io.enabled);
  f.line("EASY\n5\n"); assert(f.game.phase == Phase::Difficulty);
  f.line("medium\r\n0\n1"); f.until(Phase::HumanReady); f.line("\n"); assert(discs(f.game.board) == 0);
  const auto before = f.game.board;
  for (const char *bad : {"0\n","8\n","-1\n","1.0\n","01\n","1 2\n","1x\n","hard\n","\n"," \n"}) f.line(bad);
  f.line(std::string(80,'1')+"\n"); f.line(std::string("1\0",2)+"\n"); f.line("1\x80\n");
  assert(equal(f.game.board,before) && f.game.phase == Phase::HumanReady && f.game.difficulty == 1);
  for (int c = 0; c < 7; ++c) f.pulse(uint8_t(c));
  assert(!f.sensors.enabled && equal(f.game.board,before));
  const auto count = f.events.boards; f.line("board\nhelp\n"); assert(f.events.boards == count+1);
  for (int i = 0; i < 6; ++i) f.game.board.drop(0,i%2 ? 'X' : 'O');
  const auto full = f.game.board; f.line("1\n"); assert(equal(f.game.board,full));
  f.line("2\r\n3\n"); assert(discs(f.game.board) == 7 && f.game.board.cells[5][1] == 'O');
  for (const char *level : {"easy","medium","hard"}) {
    Fixture robot; robot.start("1",level); robot.until(Phase::IndexerRelease);
    assert(robot.game.human == 'X' && robot.game.robot == 'O' && discs(robot.game.board) == 0);
    assert(robot.game.pendingColumn == 3 && !robot.io.pulses[3]);
    finishRobot(robot); assert(robot.game.board.cells[5][3] == 'O');
    robot.line("1\n"); assert(robot.game.board.cells[5][0] == 'X');
  }
}
static void testScheduler() {
  FakeIO io; HatchSequence seq(io);
  seq.begin(0x7f,0);
  assert(seq.command(7,90,0));
  for (uint32_t t = 0; t < 600; ++t) { io.time = t; seq.tick(t); assert(seq.busy()); }
  io.time = 600; seq.tick(600); assert(!seq.busy() && !io.enabled && io.writes.size() == 8);
  for (int c = 0; c < 7; ++c) assert(io.writes[c+1].channel == c && io.writes[c+1].time == uint32_t(c*50));
  for (bool pulse : io.pulses) assert(!pulse);
  // Hatch begin must preserve an independently active indexer and its deadline.
  assert(seq.command(7,90,1000)); seq.begin(0,1100); seq.tick(1100);
  assert(io.pulses[7]); seq.tick(1300); assert(!io.pulses[7] && io.enabled);
  seq.disable(); assert(!io.enabled); for (bool pulse : io.pulses) assert(!pulse);
  seq.begin(0,UINT32_MAX-100);
  for (uint32_t dt = 0; dt <= 600; ++dt) seq.tick(uint32_t(UINT32_MAX-100+dt));
  assert(!seq.busy() && !io.enabled);
  assert(seq.openTarget(0,4000)); seq.tick(5599); assert(io.pulses[0]); seq.tick(5600); assert(!io.pulses[0]);
  assert(seq.command(7,90,6000)); io.fail = true; seq.tick(6300); assert(seq.faulted && !io.enabled);
}
static void testSequencing() {
  Fixture f; f.humanStart();
  assert(f.io.writes.size() == 14);
  f.line("4\n"); const auto before = f.game.board; const auto offset = f.io.writes.size();
  f.until(Phase::IndexerLoading);
  assert(equal(before,f.game.board) && f.events.searches == 1 && indexWrites(f,90) == 1 && indexWrites(f,0) == 0);
  for (int c = 0; c < 7; ++c) assert(f.io.writes[offset+c].angle == 0);
  assert(f.io.writes[offset+7].channel == f.game.pendingColumn && f.io.writes[offset+7].angle == 90);
  const uint32_t loadAt = f.io.writes.back().time;
  f.until(Phase::IndexerRelease);
  assert(f.io.writes.back().channel == 7 && f.io.writes.back().angle == 0);
  assert(f.io.writes.back().time-loadAt >= 800 && equal(before,f.game.board));
  finishRobot(f);
  const auto after = f.game.board;
  f.advance(4000); assert(equal(after,f.game.board) && indexWrites(f) == 2 && !f.io.enabled);
  // New robot turn cannot reuse a prior human pulse/history.
  f.pulse(0); f.line("1\n"); f.until(Phase::IndexerRelease);
  const auto pending = f.game.board; f.advance(3010);
  assert(f.game.phase == Phase::Paused && equal(pending,f.game.board) && indexWrites(f) == 4);
  Fixture full; full.humanStart();
  for (int r = 0; r < 6; ++r) full.game.board.drop(0,r%2 ? 'X' : 'O');
  full.line("4\n"); full.until(Phase::IndexerRelease); finishRobot(full);
  assert(full.io.writes[full.io.writes.size()-7].channel == 0 && full.io.writes[full.io.writes.size()-7].angle == 0);
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
    assert(f.game.phase == Phase::Paused && equal(before,f.game.board) && !f.io.enabled && indexWrites(f) == 2);
    f.advance(4000); assert(indexWrites(f) == 2 && equal(before,f.game.board));
  }
  Fixture early; early.humanStart(); early.line("4\n"); early.until(Phase::IndexerLoading);
  const auto before = early.game.board; early.pulse(uint8_t(early.game.pendingColumn));
  assert(early.game.phase == Phase::Paused && equal(before,early.game.board) && indexWrites(early,0) == 0);
  Fixture crossing; crossing.humanStart(); crossing.line("4\n"); crossing.until(Phase::IndexerLoading);
  // A pre-release LOW must not become eligible merely because its clear edge is later.
  crossing.edge(uint8_t(crossing.game.pendingColumn),true); crossing.advance(1);
  crossing.until(Phase::Paused); assert(indexWrites(crossing,0) == 0);
  Fixture active; active.humanStart(); active.edge(0,true); active.line("4\n"); active.until(Phase::Paused);
  assert(indexWrites(active) == 0);
  // Repeated short noise prevents baseline clear without feeding.
  Fixture baseline; baseline.humanStart(); baseline.line("4\n"); baseline.until(Phase::RobotBaseline);
  for (int i = 0; i < 60 && baseline.game.phase != Phase::Paused; ++i) { baseline.pulse(0,1); baseline.advance(30); }
  assert(baseline.game.phase == Phase::Paused && indexWrites(baseline) == 0);
}
struct Observer : SensorObserver {
  unsigned detects = 0, passages = 0, errors = 0;
  uint32_t start = 0;
  void detected(uint8_t, uint32_t at) override { ++detects; start = at; }
  void passage(uint8_t, uint32_t at) override { ++passages; assert(at == start); }
  void sensorProblem(const char *) override { ++errors; }
};
static void testQualification() {
  FakeSensors port; SensorService service(port); Observer observer;
  service.reset(true);
  port.edge(0,true,1000); port.edge(0,false,1500); service.poll(22000,observer);
  assert(observer.detects == 0 && observer.passages == 0);
  port.edge(0,true,30000); port.edge(0,false,35000); port.edge(0,true,40000); port.edge(0,false,45000);
  service.poll(65000,observer); assert(observer.detects == 1 && observer.passages == 1 && observer.start == 30000);
  // Future queued noise cannot satisfy quiet time using unsigned underflow.
  port.edge(1,true,100001); port.edge(1,false,100500);
  assert(!service.seal(100000,observer) && service.enabled);
  assert(!service.seal(200499,observer)); assert(service.seal(200500,observer));
  // Microsecond wraparound must preserve qualification and clear deadlines.
  FakeSensors wrapped; wrapped.timeUs = UINT32_MAX-10000;
  SensorService wrap(wrapped); Observer seen; wrap.reset(true);
  wrapped.edge(0,true,UINT32_MAX-5000); wrapped.edge(0,false,1000);
  wrap.poll(21000,seen); assert(seen.detects == 1 && seen.passages == 1 && seen.errors == 0);
  struct Streaming : FakeSensors {
    unsigned pops = 0;
    bool pop(SensorEdge &edge) override {
      if (!capturing) return false;
      edge = {0,(pops%2)==0,timeUs+pops*10}; ++pops; return true;
    }
  } stream;
  FakeIO outputs; HatchSequence scheduler(outputs); SensorService storm(stream); Events log;
  Controller controller(scheduler,storm,log);
  controller.restart(); controller.command("confirm-clear",0); controller.command("free",0); controller.command("easy",0); controller.command("1",0);
  for (uint32_t t = 0; t < 1000 && controller.phase != Phase::Paused; ++t) {
    stream.timeUs = t*1000; log.time = t; controller.tick(t);
  }
  assert(controller.phase == Phase::Paused && stream.pops <= Config::EDGE_QUEUE_SIZE+1);
  assert(discs(controller.board) == 0 && !outputs.enabled);
  for (const auto &write : outputs.writes) assert(write.channel != 7);
}
static void testRecoveryAndInterruptions() {
  const Phase phases[] = {Phase::HumanOpening,Phase::HumanReady,Phase::ClosingForRobot,Phase::RobotSearch,
    Phase::RobotOpening,Phase::RobotBaseline,Phase::IndexerLoading,Phase::IndexerRelease,
    Phase::RobotConfirm,Phase::RobotQuiet,Phase::RobotClosing};
  for (Phase phase : phases) for (int action : {0,1,2}) {
    Fixture f; f.start();
    if (phase == Phase::HumanOpening || phase == Phase::HumanReady) f.until(phase);
    else {
      f.until(Phase::HumanReady); f.line("4\n");
      if (phase == Phase::RobotQuiet || phase == Phase::RobotClosing) { f.until(Phase::IndexerRelease); f.pulse(uint8_t(f.game.pendingColumn)); }
      f.until(phase);
    }
    const auto before = f.game.board; const int pending = f.game.pendingColumn; const auto writes = f.io.writes.size();
    if (action == 2) f.game.fault(); else f.line(action == 1 ? "restart\n" : "stop\n"); f.advance(4000);
    assert(f.game.phase == (action == 2 ? Phase::Fault : action == 1 ? Phase::AwaitClear : Phase::Stopped) && equal(before,f.game.board));
    assert(f.game.pendingColumn == pending && f.io.writes.size() == writes && !f.io.enabled && !f.sensors.enabled);
    f.line("confirm-clear\n");
    if (action == 1) assert(f.game.phase == Phase::ModeSelect && discs(f.game.board) == 0 && f.game.pendingColumn == -1);
  }
  Fixture recovery; toRelease(recovery); recovery.advance(3010);
  const auto before = recovery.game.board; const auto feed = indexWrites(recovery); const int target = recovery.game.pendingColumn;
  recovery.line("arm-manual\n"); assert(recovery.game.phase == Phase::Paused);
  recovery.line("correct\n"); recovery.pulse(uint8_t(target)); assert(equal(before,recovery.game.board));
  recovery.line("arm-manual\n"); recovery.until(Phase::ManualWait); recovery.pulse(uint8_t(target));
  recovery.until(Phase::AwaitCorrection); assert(equal(before,recovery.game.board) && indexWrites(recovery) == feed);
  recovery.line("confirm-correction\n"); recovery.until(Phase::HumanReady);
  assert(discs(recovery.game.board) == discs(before)+1 && indexWrites(recovery) == feed);
  recovery.line("confirm-correction\n"); assert(discs(recovery.game.board) == discs(before)+1);
  for (Phase phase : {Phase::Correction,Phase::ManualBaseline,Phase::ManualWait,Phase::AwaitCorrection}) {
    Fixture cancel; toRelease(cancel); cancel.advance(3010); cancel.line("correct\n");
    if (phase != Phase::Correction) cancel.line("arm-manual\n");
    if (phase == Phase::ManualWait || phase == Phase::AwaitCorrection) cancel.until(Phase::ManualWait);
    if (phase == Phase::AwaitCorrection) { cancel.pulse(uint8_t(cancel.game.pendingColumn)); cancel.until(phase); }
    const auto stored = cancel.game.board; const auto index = indexWrites(cancel);
    cancel.line("restart\n"); cancel.advance(4000);
    assert(cancel.game.phase == Phase::AwaitClear && equal(stored,cancel.game.board) && indexWrites(cancel) == index && !cancel.sensors.enabled);
  }
  Fixture extra; toRelease(extra); extra.advance(3010); extra.line("correct\narm-manual\n"); extra.until(Phase::ManualWait);
  extra.pulse(uint8_t(extra.game.pendingColumn)); extra.until(Phase::AwaitCorrection); extra.pulse(uint8_t(extra.game.pendingColumn));
  assert(extra.game.phase == Phase::Paused && discs(extra.game.board) == 1 && indexWrites(extra) == 2);
  Fixture fault; toRelease(fault); const auto faultBoard = fault.game.board; fault.io.fail = true; fault.advance(400);
  assert(fault.game.phase == Phase::Fault && !fault.io.enabled);
  fault.line("restart\nconfirm-clear\ncorrect\narm-manual\n"); assert(fault.game.phase == Phase::Fault && equal(faultBoard,fault.game.board));
}
static Fixture *interruptFixture = nullptr;
static bool stopSearch() { interruptFixture->game.command("stop",interruptFixture->time); return false; }
static bool restartSearch() { interruptFixture->game.restart(); return true; }
static void testSearchCancellation() {
  for (auto hook : {stopSearch,restartSearch}) {
    Fixture f; f.humanStart("hard"); f.line("4\n"); f.until(Phase::RobotSearch);
    interruptFixture = &f; f.game.keepSearching = hook; f.tick();
    assert(f.game.phase == (hook == stopSearch ? Phase::Stopped : Phase::AwaitClear) && indexWrites(f) == 0 && !f.io.enabled);
  }
}
static void testEndings() {
  Fixture win; win.humanStart(); for (int c = 0; c < 3; ++c) win.game.board.drop(c,'O');
  win.line("4\n"); const auto final = win.game.board; win.until(Phase::Ended);
  win.advance(4000); win.line("5\n"); assert(equal(final,win.game.board) && win.events.searches == 0 && indexWrites(win) == 0);
  Fixture robot; robot.start("1"); for (int c = 0; c < 3; ++c) robot.game.board.drop(c,'O');
  robot.until(Phase::IndexerRelease); robot.pulse(uint8_t(robot.game.pendingColumn)); robot.until(Phase::Ended);
  assert(robot.game.result == Game::Result::OWins && discs(robot.game.board) == 4);
  Fixture draw; draw.humanStart(); draw.game.board = drawBoard(); draw.game.board.cells[0][0] = '.';
  draw.line("1\n"); draw.until(Phase::Ended); assert(draw.game.result == Game::Result::Draw && indexWrites(draw) == 0);
}
int main() {
  testBoard(); testAI(); testMenusAndInput(); testScheduler(); testSequencing(); testQualification();
  testSensorFailures(); testRecoveryAndInterruptions(); testSearchCancellation(); testEndings();
  puts("PASS: game/AI/parser, shared outputs, robot IR qualification, failure pauses, recovery and exactly-once commitment.");
  return 0;
}
