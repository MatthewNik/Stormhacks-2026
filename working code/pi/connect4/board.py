"""Read-only tactical analysis. Public columns are numbered 1 through 7."""
from functools import lru_cache

EMPTY = tuple("......." for _ in range(6))
ORDER = (4, 3, 5, 2, 6, 1, 7)
WIN = 1_000_000


def other(symbol):
    return "X" if symbol == "O" else "O"


def legal(board):
    return [c for c in ORDER if board[0][c - 1] == "."]


def drop(board, column, symbol):
    if symbol not in ("O", "X") or column not in legal(board):
        raise ValueError("Illegal move")
    rows = list(board)
    for r in range(5, -1, -1):
        if rows[r][column - 1] == ".":
            rows[r] = rows[r][:column - 1] + symbol + rows[r][column:]
            return tuple(rows)
    raise ValueError("Full column")


def result(board):
    for r in range(6):
        for c in range(7):
            symbol = board[r][c]
            if symbol == ".":
                continue
            for dr, dc in ((0, 1), (1, 0), (1, 1), (1, -1)):
                if 0 <= r + 3 * dr < 6 and 0 <= c + 3 * dc < 7:
                    if all(board[r + i * dr][c + i * dc] == symbol for i in range(1, 4)):
                        return symbol + "_wins"
    return "playing" if legal(board) else "draw"


def replay(moves):
    board = EMPTY
    for index, move in enumerate(moves):
        if result(board) != "playing" or move["symbol"] != ("O" if index % 2 == 0 else "X"):
            raise ValueError("Invalid move history")
        board = drop(board, move["column"], move["symbol"])
    return board


def winning_columns(board, symbol):
    if result(board) != "playing":
        return []
    return [c for c in legal(board) if result(drop(board, c, symbol)) == symbol + "_wins"]


def fork_columns(board, symbol):
    forks = []
    if result(board) != "playing":
        return forks
    for c in legal(board):
        child = drop(board, c, symbol)
        if result(child) == "playing" and not winning_columns(child, other(symbol)):
            if len(winning_columns(child, symbol)) >= 2:
                forks.append(c)
    return forks


def heuristic(board, perspective):
    enemy = other(perspective)
    score = sum(6 if row[3] == perspective else -6 if row[3] == enemy else 0 for row in board)
    weights = (0, 1, 12, 100, 10000)
    for r in range(6):
        for c in range(7):
            for dr, dc in ((0, 1), (1, 0), (1, 1), (1, -1)):
                if not (0 <= r + 3 * dr < 6 and 0 <= c + 3 * dc < 7):
                    continue
                window = [board[r + i * dr][c + i * dc] for i in range(4)]
                ours, theirs = window.count(perspective), window.count(enemy)
                if not theirs:
                    score += weights[ours]
                if not ours:
                    score -= weights[theirs]
    return score


def minimax(board, depth, ply, turn, perspective, alpha, beta):
    outcome = result(board)
    if outcome == "draw":
        return 0
    if outcome != "playing":
        return WIN - ply if outcome == perspective + "_wins" else -WIN + ply
    if depth == 0:
        return heuristic(board, perspective)
    maximizing = turn == perspective
    best = -2 * WIN if maximizing else 2 * WIN
    for column in legal(board):
        value = minimax(drop(board, column, turn), depth - 1, ply + 1,
                        other(turn), perspective, alpha, beta)
        if maximizing:
            best, alpha = max(best, value), max(alpha, value)
        else:
            best, beta = min(best, value), min(beta, value)
        if alpha >= beta:
            break
    return best


@lru_cache(maxsize=128)
def scores(board, symbol, depth=5):
    if result(board) != "playing":
        return ()
    return tuple((c, minimax(drop(board, c, symbol), depth - 1, 1, other(symbol), symbol,
                             -2 * WIN, 2 * WIN)) for c in legal(board))


def analyse(board, symbol):
    values = dict(scores(tuple(board), symbol))
    best = max(values, key=values.get) if values else None
    return {"legal_columns": legal(board), "immediate_wins": winning_columns(board, symbol),
            "opponent_wins": winning_columns(board, other(symbol)), "fork_columns": fork_columns(board, symbol),
            "recommended_column": best, "scores": values, "search_depth": 5,
            "search_is_perfect": False}


def compare_move(before, column, symbol):
    evidence = analyse(before, symbol)
    after = drop(before, column, symbol)
    values = evidence["scores"]
    return {"played_column": column, "best_column": evidence["recommended_column"],
            "score_loss": max(values.values()) - values[column],
            "won": result(after) == symbol + "_wins",
            "missed_win": bool(evidence["immediate_wins"]) and column not in evidence["immediate_wins"],
            "allowed_immediate_win": winning_columns(after, other(symbol)),
            "created_fork": column in evidence["fork_columns"], "evidence": evidence}
