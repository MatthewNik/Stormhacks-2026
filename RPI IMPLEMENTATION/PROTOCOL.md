# USB protocol v1

USB UART is 115200 baud. ESP32 emits UTF-8 JSON objects, one per newline. Every frame includes `v: 1` and `type`. ESP32 ROM boot output is outside this protocol and is ignored by the Pi. Only the Pi terminal process should own USB serial.

## State frames

`snapshot`, `state`, and `move` have the same full-state shape. Their type indicates why a frame was emitted; the authoritative committed history is always `moves`. Never infer commitment from a message, search result, target selection or timeout.

```json
{"v":1,"type":"snapshot","boot_id":42,"game_id":1,"revision":9,"move_number":1,"mode":"coach","difficulty":1,"robot_first":false,"human":"O","robot":"X","phase":"IndexerRelease","pending_column":4,"result":"playing","dropped":0,"board":[".......",".......",".......",".......",".......","O......"],"moves":[{"column":1,"symbol":"O"}]}
```

- `boot_id`: random unsigned 32-bit boot identifier. `game_id`: unsigned 32-bit counter incremented by `confirm-clear`; zero means no confirmed game yet.
- `revision`: unsigned 32-bit revision. Compare with wraparound handling within one boot/game. `move_number` is the history length, at most 42.
- `mode`: `free` or `coach`; `difficulty`: -1 before selection, otherwise 0/1/2 for Easy/Medium/Hard. `robot_first` identifies the chosen starter.
- `human`/`robot`: O/X assignments. O always starts. `board`: six strings of seven cells, top row first; `.` is empty.
- `phase`: controller enum name, for example `AwaitClear`, `ModeSelect`, `Difficulty`, `FirstPlayer`, `HumanReady`, `IndexerRelease`, `Paused`, `Fault`, or `Ended`.
- `pending_column`: zero when none, otherwise a public column 1–7. `result`: `playing`, `O_wins`, `X_wins`, or `draw`.
- `dropped`: count of discarded whole telemetry frames. `moves`: ordered column/symbol objects for committed moves only.

The Pi replays history and requires an exact board/result match before accepting it. Deduplicate moves by boot ID, game ID and move number. Replay against prior logs must have the same committed prefix.

ESP32 has four 2048-byte transmission slots. A full queue drops whole frames without truncating a frame already being transmitted. A later snapshot supplies current state and complete current-game history. Each loop sends at most 32 bytes and reads at most 64 bytes; network/API/audio work never runs on ESP32.

## Commands to ESP32

Input uses a deliberately small ASCII line grammar, rather than a JSON parser on the real-time controller. Lines may end in LF, CR or CRLF; the maximum is 127 bytes before the delimiter. Existing operator commands and raw single-digit human columns remain available for direct Serial Monitor use.

```text
snapshot
move <boot_id> <game_id> <expected_move_number> <request_id> <column>
status <request_id>
```

All move fields are unsigned decimal 32-bit integers separated by single spaces. Columns are 1–7. No extra arguments are allowed. A move must begin and finish while ESP32 is `HumanReady`, refer to the current boot/game/turn, and choose a legal column. ESP32 commits through its normal human command path. Accepted request IDs are remembered for the current game; repeated IDs never commit another move.

ESP32 replies with `{"v":1,"type":"command_result","request_id":123,"status":"accepted"}`. Status can also be `duplicate`, `stale_game`, `stale_turn`, `not_ready`, `illegal_column`, `not_accepted`, or `malformed`. A malformed line without a usable ID replies with ID zero. `status` returns `duplicate` for an already accepted request, otherwise `not_accepted`; it never executes a move.

The Pi tags moves, sends each once, discards unsent commands on disconnect, and polls snapshots every two seconds. An explicit `snapshot` while a request is unresolved also sends `status` after prior queued commands. Never automatically replay an uncertain move.

Setup/operator commands: `confirm-clear`, `free`, `coach`, `easy`, `medium`, `hard`, `0`/`1` at starter selection, `help`, `board`, `stop`, `restart`, `correct`, `arm-manual`, and `confirm-correction`. ESP32 rejects commands inappropriate to its phase. Pi/API failures cannot clear faults or bypass robot sensor confirmation.

## Other frames

- `message`: `text`, JSON-escaped operator information.
- `search`: `column`, `duration_ms`, `nodes`; diagnostic only, never a committed move.
- `command_result`: `request_id`, `status`.

Provider outputs never enter this command channel. Gemini receives read-only board/history/evidence; its structured response contains only `text`, `move_number`, and optional `suggested_column`.
