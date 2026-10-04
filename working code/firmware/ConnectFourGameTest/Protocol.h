#pragma once
#include "Controller.h"
#include <stdio.h>
#include <stdarg.h>

inline const char *phaseName(Phase p) {
  switch (p) {
#define PHASE_NAME(name) case Phase::name: return #name
    PHASE_NAME(AwaitClear); PHASE_NAME(ModeSelect); PHASE_NAME(FirstPlayer); PHASE_NAME(Difficulty);
    PHASE_NAME(HumanOpening); PHASE_NAME(HumanReady); PHASE_NAME(ClosingForRobot); PHASE_NAME(RobotSearch);
    PHASE_NAME(RobotOpening); PHASE_NAME(RobotBaseline); PHASE_NAME(IndexerLoading); PHASE_NAME(IndexerRelease);
    PHASE_NAME(RobotConfirm); PHASE_NAME(RobotQuiet); PHASE_NAME(RobotClosing); PHASE_NAME(Paused);
    PHASE_NAME(Correction); PHASE_NAME(ManualBaseline); PHASE_NAME(ManualWait); PHASE_NAME(AwaitCorrection);
    PHASE_NAME(Stopped); PHASE_NAME(Fault); PHASE_NAME(Ended); PHASE_NAME(EndClosing);
    PHASE_NAME(StartupPositioning); PHASE_NAME(HumanBaseline); PHASE_NAME(HumanConfirm); PHASE_NAME(IndexerReset);
#undef PHASE_NAME
  }
  return "Unknown";
}
inline const char *resultName(Game::Result r) {
  return r == Game::Result::OWins ? "O_wins" : r == Game::Result::XWins ? "X_wins" : r == Game::Result::Draw ? "draw" : "playing";
}
struct JsonFrame {
  static constexpr unsigned CAPACITY = 2048;
  char data[CAPACITY] = {};
  unsigned size = 0;
  bool valid = true;
  void add(const char *format, ...) {
    if (!valid) return;
    va_list args; va_start(args,format);
    const int n = vsnprintf(data+size,CAPACITY-size,format,args); va_end(args);
    if (n < 0 || unsigned(n) >= CAPACITY-size) { valid = false; return; }
    size += unsigned(n);
  }
  void quoted(const char *value) {
    add("\"");
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
      if (*p == '"' || *p == '\\') add("\\%c",*p);
      else if (*p < 32) add("\\u%04x",unsigned(*p));
      else add("%c",*p);
    }
    add("\"");
  }
};
// Drop whole frames on overflow; preserve any frame already partly transmitted.
class FrameQueue {
  JsonFrame frames[4];
  unsigned head = 0, count = 0, offset = 0;
public:
  uint32_t dropped = 0;
  bool room() const { return count < 4; }
  bool push(const JsonFrame &f) {
    if (!f.valid || !room()) { ++dropped; return false; }
    frames[(head+count)%4] = f; ++count; return true;
  }
  const char *front(unsigned &remaining) const {
    remaining = count ? frames[head].size-offset : 0;
    return count ? frames[head].data+offset : nullptr;
  }
  void consume(unsigned bytes) {
    if (!count) return;
    const unsigned remaining = frames[head].size-offset;
    offset += bytes < remaining ? bytes : remaining;
    if (offset == frames[head].size) { head = (head+1)%4; --count; offset = 0; }
  }
};
inline JsonFrame stateFrame(const Controller &g, const char *type, uint32_t dropped) {
  JsonFrame f;
  f.add("{\"v\":1,\"type\":\"%s\",\"boot_id\":%lu,\"game_id\":%lu,\"revision\":%lu,\"move_number\":%u,",
    type,(unsigned long)g.bootId,(unsigned long)g.gameId,(unsigned long)g.revision,unsigned(g.moveCount));
  f.add("\"mode\":\"%s\",\"difficulty\":%d,\"robot_first\":%s,\"human\":\"%c\",\"robot\":\"%c\",",
    g.mode == Mode::Coach ? "coach" : "free",g.difficulty,g.robotFirst ? "true" : "false",g.human,g.robot);
  f.add("\"phase\":\"%s\",\"pending_column\":%d,\"result\":\"%s\",\"dropped\":%lu,\"board\":[",
    phaseName(g.phase),g.pendingColumn+1,resultName(g.result),(unsigned long)dropped);
  for (int r = 0; r < 6; ++r) f.add("%s\"%.7s\"",r ? "," : "",g.board.cells[r]);
  f.add("],\"moves\":[");
  for (unsigned i = 0; i < g.moveCount; ++i)
    f.add("%s{\"column\":%u,\"symbol\":\"%c\"}",i ? "," : "",unsigned(g.history[i].column),g.history[i].symbol);
  f.add("]}\n"); return f;
}
struct ProtocolReply {
  virtual ~ProtocolReply() = default;
  virtual void snapshot() = 0;
  virtual void commandResult(uint32_t request, const char *status) = 0;
  virtual void diagnostics() {}
};
class ProtocolReader {
  char buffer[128] = {};
  unsigned length = 0;
  bool invalid = false, beganReady = false;
  uint32_t accepted[42] = {}, observedGame = UINT32_MAX;
  unsigned acceptedCount = 0;
  static bool numbers(const char *line, uint32_t (&values)[5]) {
    const char *p = line+5;
    for (unsigned i = 0; i < 5; ++i) {
      if (*p < '0' || *p > '9') return false;
      uint32_t n = 0;
      while (*p >= '0' && *p <= '9') {
        const unsigned digit = unsigned(*p++-'0');
        if (n > (UINT32_MAX-digit)/10) return false;
        n = n*10+digit;
      }
      values[i] = n;
      if (i < 4) { if (*p++ != ' ') return false; }
    }
    return !*p;
  }
public:
  void feed(char ch, Controller &g, uint32_t now, GameEvents &events, ProtocolReply &reply) {
    if (ch != '\n' && ch != '\r') {
      if (!length && !invalid) beganReady = g.phase == Phase::HumanReady;
      const unsigned char c = static_cast<unsigned char>(ch);
      if ((c < 32 && ch != '\t') || c >= 127 || length >= sizeof(buffer)-1) invalid = true;
      else if (!invalid) buffer[length++] = ch;
      return;
    }
    buffer[length] = 0;
    while (length && (buffer[length-1] == ' ' || buffer[length-1] == '\t')) buffer[--length] = 0;
    const char *line = buffer; while (*line == ' ' || *line == '\t') ++line;
    if (invalid) events.message("ERROR: oversized or non-ASCII command ignored.");
    else if (!strcmp(line,"snapshot")) {
      reply.snapshot();
      if (g.phase == Phase::Fault) reply.diagnostics();
    }
    else if (!strcmp(line,"diagnose")) { reply.diagnostics(); reply.snapshot(); }
    else if (!strncmp(line,"status ",7)) {
      uint32_t request = 0;
      const char *p = line+7;
      bool valid = *p >= '0' && *p <= '9';
      while (*p >= '0' && *p <= '9') {
        const unsigned digit = unsigned(*p++-'0');
        if (request > (UINT32_MAX-digit)/10) { valid = false; break; }
        request = request*10+digit;
      }
      bool acceptedRequest = false;
      if (observedGame == g.gameId) for (unsigned i = 0; i < acceptedCount; ++i)
        if (accepted[i] == request) acceptedRequest = true;
      reply.commandResult(valid && !*p ? request : 0, valid && !*p ? acceptedRequest ? "duplicate" : "not_accepted" : "malformed");
      reply.snapshot();
    }
    else if (!strncmp(line,"move ",5)) {
      uint32_t v[5] = {};
      if (!numbers(line,v)) reply.commandResult(0,"malformed");
      else {
        const uint32_t request = v[3];
        if (observedGame != g.gameId) { observedGame = g.gameId; acceptedCount = 0; }
        bool duplicate = false;
        for (unsigned i = 0; i < acceptedCount; ++i) if (accepted[i] == request) duplicate = true;
        const char *status = "accepted";
        if (v[0] != g.bootId || v[1] != g.gameId) status = "stale_game";
        else if (duplicate) status = "duplicate";
        else if (v[2] != g.moveCount) status = "stale_turn";
        else if (!beganReady || g.phase != Phase::HumanReady) status = "not_ready";
        else if (v[4] < 1 || v[4] > 7 || !g.board.legal(int(v[4])-1)) status = "illegal_column";
        else status = "sensor_only";
        reply.commandResult(request,status);
      }
    } else if (*line) {
      if (!beganReady && strlen(line) == 1 && *line >= '1' && *line <= '7' && g.phase == Phase::HumanReady)
        events.message("ERROR: move began before human hatches were ready.");
      else g.command(line,now);
      reply.snapshot();
    }
    length = 0; invalid = false;
  }
};
