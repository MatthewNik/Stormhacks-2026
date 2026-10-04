#define main existing_native_main
#include "native_tests.cpp"
#undef main
#include "../Buttons.h"
#include "../Protocol.h"

struct Replies : ProtocolReply {
  unsigned snapshots = 0;
  uint32_t id = 0;
  std::string status;
  void snapshot() override { ++snapshots; }
  void commandResult(uint32_t value, const char *text) override { id = value; status = text; }
};
static void protocolLine(ProtocolReader &reader, Replies &reply, Fixture &f, const std::string &line) {
  f.clock(); for (char c : line) reader.feed(c,f.game,f.time,f.events,reply);
}
static void menuInput(MenuButtons &buttons, Fixture &f, bool left, bool right, bool centre, unsigned duration) {
  while (duration--) { f.clock(); buttons.poll(left,right,centre,f.game,f.time); ++f.time; }
}
static void click(MenuButtons &buttons, Fixture &f, int button, unsigned duration = 60) {
  menuInput(buttons,f,button == 0,button == 1,button == 2,duration);
  menuInput(buttons,f,false,false,false,40);
}
static void testButtons() {
  assert(Config::BUTTON_LEFT == 13 && Config::BUTTON_RIGHT == 14 && Config::BUTTON_CENTRE == 23);
  Fixture f; MenuButtons buttons;
  menuInput(buttons,f,false,false,false,40); click(buttons,f,2);
  assert(f.game.phase == Phase::AwaitClear); // clearing stays a terminal acknowledgement
  f.game.command("confirm-clear",f.time); menuInput(buttons,f,false,false,false,40);
  assert(f.game.highlightedMode == Mode::FreePlay);
  click(buttons,f,1,10); assert(f.game.highlightedMode == Mode::FreePlay); // bounce
  click(buttons,f,1,1500);
  assert(f.game.highlightedMode == Mode::Coach && f.game.mode == Mode::FreePlay && f.game.phase == Phase::ModeSelect);
  click(buttons,f,1); assert(f.game.highlightedMode == Mode::FreePlay);
  click(buttons,f,0); assert(f.game.highlightedMode == Mode::Coach);
  click(buttons,f,0); assert(f.game.highlightedMode == Mode::FreePlay);
  for (int pair = 0; pair < 3; ++pair) {
    menuInput(buttons,f,pair != 0,pair != 1,pair != 2,60);
    menuInput(buttons,f,false,false,true,60);
    menuInput(buttons,f,false,false,false,40);
    assert(f.game.phase == Phase::ModeSelect && f.game.highlightedMode == Mode::FreePlay);
  }
  menuInput(buttons,f,false,false,true,1500);
  assert(f.game.phase == Phase::Difficulty && f.game.difficulty == 0);
  menuInput(buttons,f,false,false,true,1500); assert(f.game.phase == Phase::Difficulty);
  menuInput(buttons,f,false,false,false,40);
  click(buttons,f,0); assert(f.game.difficulty == 2);
  click(buttons,f,0); assert(f.game.difficulty == 1);
  click(buttons,f,0); assert(f.game.difficulty == 0);
  click(buttons,f,1,1500); assert(f.game.difficulty == 1);
  click(buttons,f,1); assert(f.game.difficulty == 2);
  click(buttons,f,1); assert(f.game.difficulty == 0);
  click(buttons,f,2); assert(f.game.phase == Phase::FirstPlayer && !f.game.robotFirst);
  click(buttons,f,0); assert(f.game.robotFirst);
  click(buttons,f,0); assert(!f.game.robotFirst);
  click(buttons,f,1); assert(f.game.robotFirst);
  click(buttons,f,1); assert(!f.game.robotFirst);
  click(buttons,f,2); assert(f.game.phase == Phase::RobotClosing);
  f.until(Phase::HumanReady);
  click(buttons,f,0); click(buttons,f,1); click(buttons,f,2);
  assert(f.game.phase == Phase::HumanReady && f.game.moveCount == 0);

  Fixture coach; MenuButtons coachButtons;
  coach.game.command("confirm-clear",0); menuInput(coachButtons,coach,false,false,false,40);
  click(coachButtons,coach,1); click(coachButtons,coach,2);
  assert(coach.game.mode == Mode::Coach && coach.game.difficulty == 1 && !coach.game.robotFirst);
  coach.until(Phase::HumanReady);

  Fixture robot; MenuButtons robotButtons;
  robot.game.command("confirm-clear",0); menuInput(robotButtons,robot,false,false,false,40);
  click(robotButtons,robot,2); click(robotButtons,robot,2); click(robotButtons,robot,1); click(robotButtons,robot,2);
  assert(robot.game.human == 'X' && robot.game.robot == 'O'); robot.until(Phase::IndexerRelease);

  DebouncedButton wrap;
  assert(wrap.poll(true,UINT32_MAX-20) == ButtonAction::None);
  assert(wrap.poll(true,20) == ButtonAction::Press);
  assert(wrap.poll(true,1500) == ButtonAction::None);
  assert(wrap.poll(false,1510) == ButtonAction::None);
  assert(wrap.poll(false,1545) == ButtonAction::None);
  assert(wrap.poll(true,1550) == ButtonAction::None);
  assert(wrap.poll(true,1585) == ButtonAction::Press);
}
static void testProtocol() {
  Fixture f; f.game.bootId = 42; f.humanStart();
  ProtocolReader reader; Replies reply;
  protocolLine(reader,reply,f,"snapshot\n"); assert(reply.snapshots == 1);
  protocolLine(reader,reply,f,"move 41 1 0 10 4\n"); assert(reply.status == "stale_game" && f.game.moveCount == 0);
  protocolLine(reader,reply,f,"move 42 1 1 10 4\n"); assert(reply.status == "stale_turn");
  protocolLine(reader,reply,f,"move 42 1 0 10 8\n"); assert(reply.status == "illegal_column");
  protocolLine(reader,reply,f,"move 42 1 0 10 4 extra\n"); assert(reply.status == "malformed");
  protocolLine(reader,reply,f,"move 4294967296 1 0 10 4\n"); assert(reply.status == "malformed");
  protocolLine(reader,reply,f,"move 42 1 0 10 4\n"); assert(reply.status == "accepted" && f.game.moveCount == 1);
  assert(f.game.history[0].column == 4 && f.game.history[0].symbol == 'O');
  protocolLine(reader,reply,f,"move 42 1 0 10 4\n"); assert(reply.status == "duplicate" && f.game.moveCount == 1);
  protocolLine(reader,reply,f,"status 10\n"); assert(reply.status == "duplicate" && reply.id == 10);
  protocolLine(reader,reply,f,"status 999\n"); assert(reply.status == "not_accepted" && reply.id == 999);
  f.until(Phase::IndexerRelease); assert(f.game.moveCount == 1);
  finishRobot(f); assert(f.game.moveCount == 2 && f.game.history[1].symbol == 'X');
  protocolLine(reader,reply,f,"move 42 1 2 10 5\n"); assert(reply.status == "duplicate");
  protocolLine(reader,reply,f,"move 42 1 2 11 5\n"); assert(reply.status == "accepted" && f.game.moveCount == 3);
  auto state = stateFrame(f.game,"snapshot",1);
  assert(state.valid && state.size < JsonFrame::CAPACITY && strstr(state.data,"\"move_number\":3"));
  f.game.restart(); assert(f.game.moveCount == 3);
  f.game.command("confirm-clear",f.time); assert(f.game.gameId == 2 && f.game.moveCount == 0);
  Fixture early; early.start();
  protocolLine(reader,reply,early,"move 0 1 0 12 ");
  early.until(Phase::HumanReady); protocolLine(reader,reply,early,"4\n");
  assert(reply.status == "not_ready" && early.game.moveCount == 0);
}
static void testTelemetry() {
  JsonFrame frame; frame.add("{"); frame.quoted("a\"\n\\b"); frame.add("}\n");
  assert(strstr(frame.data,"\\\"") && strstr(frame.data,"\\u000a") && strstr(frame.data,"\\\\"));
  FrameQueue queue;
  for (int i = 0; i < 4; ++i) assert(queue.push(frame));
  unsigned remaining; const std::string before = queue.front(remaining);
  queue.consume(3); assert(!queue.push(frame) && queue.dropped == 1);
  assert(std::string(queue.front(remaining)) == before.substr(3));
  queue.consume(remaining); assert(queue.push(frame));
  Fixture full; full.game.moveCount = 42;
  for (int i = 0; i < 42; ++i) full.game.history[i] = {uint8_t(i%7+1),i%2 ? 'X' : 'O'};
  assert(stateFrame(full.game,"snapshot",UINT32_MAX).valid);
}
static void testCoach() {
  Fixture f; f.game.command("confirm-clear",0); f.game.command("coach",0);
  assert(f.game.mode == Mode::Coach && f.game.difficulty == 1 && !f.game.robotFirst);
  f.until(Phase::HumanReady); f.line("1\n"); f.until(Phase::IndexerRelease);
  assert(f.game.pendingColumn == 3 && f.game.moveCount == 1);
  finishRobot(f);
}
int main(int argc, char **) {
  if (argc > 1) {
    Fixture f; f.game.bootId = 42; f.humanStart();
    printf("%s",stateFrame(f.game,"snapshot",0).data);
    f.line("4\n"); f.until(Phase::IndexerRelease);
    printf("%s",stateFrame(f.game,"move",0).data);
    finishRobot(f); printf("%s",stateFrame(f.game,"snapshot",0).data);
    return 0;
  }
  testButtons(); testProtocol(); testTelemetry(); testCoach();
  puts("PASS: three-button navigation/confirmation, debounce/held/chord/release handling, turn tags, history, bounded telemetry and coach opening.");
  return 0;
}
