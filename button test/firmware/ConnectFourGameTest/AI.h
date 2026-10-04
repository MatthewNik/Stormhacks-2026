#pragma once
#include "Game.h"
#include <limits.h>

namespace AI {
struct SearchResult { int column = -1; int score = 0; uint32_t nodes = 0; bool aborted = false; };
using KeepSearching = bool (*)();
constexpr int ORDER[7] = {3,2,4,1,5,0,6};
constexpr int WIN = 1000000;
inline int heuristic(const Game::Board &board, char robot) {
  int score = 0;
  for (int r = 0; r < 6; ++r) {
    if (board.cells[r][3] == robot) score += 6;
    else if (board.cells[r][3] == Game::other(robot)) score -= 6;
  }
  const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
  const int weights[5] = {0,1,12,100,10000};
  for (int r = 0; r < 6; ++r) for (int c = 0; c < 7; ++c) for (const auto &d : dirs) {
    const int rr = r+3*d[0], cc = c+3*d[1];
    if (rr < 0 || rr >= 6 || cc < 0 || cc >= 7) continue;
    int ours = 0, theirs = 0;
    for (int i = 0; i < 4; ++i) {
      const char v = board.cells[r+i*d[0]][c+i*d[1]];
      if (v == robot) ++ours;
      else if (v == Game::other(robot)) ++theirs;
    }
    if (!theirs) score += weights[ours];
    if (!ours) score -= weights[theirs];
  }
  return score;
}
inline int minimax(const Game::Board &board, int depth, int ply, char turn, char robot,
                   int alpha, int beta, SearchResult &stats, KeepSearching keep) {
  ++stats.nodes;
  if (keep && (stats.nodes % 128 == 0) && !keep()) { stats.aborted = true; return 0; }
  const auto result = board.result();
  if (result == Game::Result::Draw) return 0;
  if (result != Game::Result::Playing) {
    const char winner = result == Game::Result::OWins ? 'O' : 'X';
    return winner == robot ? WIN - ply : -WIN + ply;
  }
  if (!depth) return heuristic(board, robot);
  const bool maximizing = turn == robot;
  int best = maximizing ? -INT_MAX : INT_MAX;
  for (int col : ORDER) if (board.legal(col)) {
    Game::Board child = board;
    child.drop(col, turn);
    const int value = minimax(child, depth-1, ply+1, Game::other(turn), robot, alpha, beta, stats, keep);
    if (stats.aborted) return 0;
    if (maximizing) { if (value > best) best = value; if (best > alpha) alpha = best; }
    else { if (value < best) best = value; if (best < beta) beta = best; }
    if (alpha >= beta) break;
  }
  return best;
}
inline SearchResult choose(const Game::Board &authoritative, char robot, int depth, KeepSearching keep = nullptr) {
  SearchResult stats;
  const Game::Board root = authoritative;
  if (root.result() != Game::Result::Playing || depth < 1) return stats;
  int best = -INT_MAX;
  for (int col : ORDER) if (root.legal(col)) {
    Game::Board child = root;
    child.drop(col, robot);
    const int score = minimax(child, depth-1, 1, Game::other(robot), robot, best, INT_MAX, stats, keep);
    if (stats.aborted) { stats.column = -1; return stats; }
    if (score > best) { best = score; stats.column = col; stats.score = score; }
  }
  return stats;
}
}
