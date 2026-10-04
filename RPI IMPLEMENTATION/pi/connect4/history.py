"""Validate ESP32 snapshots and persist deduplicated games."""
from datetime import datetime, timezone
import json
from pathlib import Path
from . import board as rules

PHASES = {"AwaitClear", "ModeSelect", "FirstPlayer", "Difficulty", "HumanOpening", "HumanReady",
          "ClosingForRobot", "RobotSearch", "RobotOpening", "RobotBaseline", "IndexerLoading",
          "IndexerRelease", "RobotConfirm", "RobotQuiet", "RobotClosing", "Paused", "Correction",
          "ManualBaseline", "ManualWait", "AwaitCorrection", "Stopped", "Fault", "Ended", "EndClosing"}
STATE_TYPES = {"snapshot", "move", "state"}


def validate_state(event):
    if event.get("v") != 1 or event.get("type") not in STATE_TYPES:
        raise ValueError("Unsupported state frame")
    for name in ("boot_id", "game_id", "revision", "move_number", "pending_column", "dropped"):
        if type(event.get(name)) is not int or not 0 <= event[name] <= 0xFFFFFFFF:
            raise ValueError("Invalid integer field")
    if event["move_number"] > 42 or event["pending_column"] > 7:
        raise ValueError("Invalid move count or column")
    if event.get("phase") not in PHASES or event.get("mode") not in ("free", "coach"):
        raise ValueError("Invalid phase or mode")
    if type(event.get("difficulty")) is not int or event["difficulty"] not in (-1, 0, 1, 2):
        raise ValueError("Invalid difficulty")
    if type(event.get("robot_first")) is not bool:
        raise ValueError("Invalid starter")
    if event.get("human") not in ("O", "X") or event.get("robot") != rules.other(event["human"]):
        raise ValueError("Invalid symbols")
    rows, moves = event.get("board"), event.get("moves")
    if not isinstance(rows, list) or len(rows) != 6 or any(
            not isinstance(row, str) or len(row) != 7 or set(row) - set(".OX") for row in rows):
        raise ValueError("Invalid board")
    if not isinstance(moves, list) or len(moves) != event["move_number"]:
        raise ValueError("Invalid history length")
    for move in moves:
        if not isinstance(move, dict) or type(move.get("column")) is not int or move.get("symbol") not in ("O", "X"):
            raise ValueError("Invalid move")
    replayed = rules.replay(moves)
    if replayed != tuple(rows) or rules.result(replayed) != event.get("result"):
        raise ValueError("History disagrees with board or result")
    return dict(event)


class GameStore:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.state = None
        self.moves = []
        self.status = None
        self.path = None

    def _append(self, event):
        if self.path is None:
            return
        record = {"saved_at": datetime.now(timezone.utc).isoformat(), **event}
        with self.path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(record, separators=(",", ":")) + "\n")
            stream.flush()

    def _status(self, status):
        if self.status != status:
            self.status = status
            self._append({"type": "session_status", "status": status})

    def accept(self, event):
        state = validate_state(event)
        key = state["boot_id"], state["game_id"]
        old_key = (self.state["boot_id"], self.state["game_id"]) if self.state else None
        changed_game = key != old_key
        if changed_game:
            if self.state and self.status != "completed":
                self._status("interrupted" if key[0] == old_key[0] else "incomplete")
            self.path = self.directory / f"game-{key[0]}-{key[1]}.jsonl" if key[1] else None
            self.moves, self.status = [], None
            if self.path and self.path.exists():
                try:
                    for line in self.path.read_text(encoding="utf-8").splitlines():
                        record = json.loads(line)
                        if record.get("type") == "committed_move":
                            if record["move_number"] != len(self.moves) + 1:
                                raise ValueError("Non-contiguous saved history")
                            self.moves.append(record["move"])
                        elif record.get("type") == "session_status":
                            self.status = record["status"]
                except (ValueError, KeyError, TypeError):
                    raise ValueError("Saved game log is damaged; preserve it for inspection") from None
            if not self.moves:
                self._append({"type": "session_start", "boot_id": key[0], "game_id": key[1]})
        elif self.state:
            delta = (state["revision"] - self.state["revision"]) & 0xFFFFFFFF
            if delta >= 0x80000000:
                return False
        if state["moves"][:len(self.moves)] != self.moves:
            raise ValueError("Snapshot conflicts with previously committed moves")
        for index in range(len(self.moves), len(state["moves"])):
            move = state["moves"][index]
            self._append({"type": "committed_move", "move_number": index + 1, "move": move})
            self.moves.append(move)
        different = self.state != state
        self.state = state
        if different:
            self._append({"type": "esp32_state", "state": state})
        status = "completed" if state["phase"] == "Ended" or self.status == "completed" else "interrupted" if state["phase"] == "AwaitClear" else "active"
        self._status(status)
        return different

    def disconnected(self):
        if self.state and self.status not in ("completed", "interrupted"):
            self._status("incomplete")

    def save_advice(self, context, text, source):
        if self.state and context[:2] == (self.state["boot_id"], self.state["game_id"]):
            self._append({"type": "advice", "move_number": context[2], "text": text, "source": source})
