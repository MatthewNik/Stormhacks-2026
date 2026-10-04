# USB protocol v1

USB UART is 115200 baud. ESP32 emits UTF-8 JSON objects, one per newline. Every frame includes `v: 1` and `type`. ESP32 ROM boot output is outside this protocol and is ignored by the Pi. Only the Pi terminal process should own USB serial.

## State frames

`snapshot`, `state`, and `move` have the same full-state shape. Their type indicates why a frame was emitted; the authoritative committed history is always `moves`. Never infer commitment from a message, search result, target selection or timeout.

```json
{"v":1,"type":"snapshot","boot_id":42,"game_id":1,"revision":9,"move_number":1,"mode":"free","difficulty":1,"robot_first":false,"human":"O","robot":"X","phase":"IndexerRelease","pending_column":4,"result":"playing","dropped":0,"board":[".......",".......",".......",".......",".......","O......"],"moves":[{"column":1,"symbol":"O"}]}
```

- `boot_id`: random unsigned 32-bit boot identifier. `game_id`: unsigned 32-bit counter incremented by `confirm-clear`; zero means no confirmed game yet.
- `revision`: unsigned 32-bit revision. Compare with wraparound handling within one boot/game. `move_number` is the history length, at most 42.
- `mode`: `free` in current gameplay (`coach` retained as a reserved legacy value); `difficulty`: -1 before selection, otherwise 0/1/2 for Easy/Medium/Hard. `robot_first` identifies the chosen starter.
- `human`/`robot`: O/X assignments. O always starts. `board`: six strings of seven cells, top row first; `.` is empty.
- `phase`: controller enum name, for example `AwaitClear`, `ModeSelect`, `Difficulty`, `FirstPlayer`, `HumanReady`, `IndexerRelease`, `Paused`, `Fault`, or `Ended`.
- `pending_column`: zero when none, otherwise a public column 1–7. `result`: `playing`, `O_wins`, `X_wins`, or `draw`.
- `dropped`: count of discarded whole telemetry frames. `moves`: ordered column/symbol objects for committed moves only.

The Pi replays history and requires an exact board/result match before accepting it. Deduplicate moves by boot ID, game ID and move number. Replay against prior logs must have the same committed prefix.

ESP32 has four 2048-byte transmission slots. A full queue drops whole frames without truncating a frame already being transmitted. A later snapshot supplies current state and complete current-game history. Each loop sends at most 32 bytes and reads at most 64 bytes; network/API/audio work never runs on ESP32.

## Commands to ESP32

Input uses a deliberately small ASCII line grammar, rather than a JSON parser on the real-time controller. Lines may end in LF, CR or CRLF; the maximum is 127 bytes before the delimiter. Operator commands remain available. Raw human column digits are rejected; 0/1 remain starter selections only on FirstPlayer.

```text
snapshot
move <boot_id> <game_id> <expected_move_number> <request_id> <column>
status <request_id>
diagnose
```

All move fields are unsigned decimal 32-bit integers separated by single spaces. Columns are 1–7. No extra arguments are allowed. A syntactically valid request for the current boot/game/turn, beginning and ending in HumanReady with a legal column, returns `sensor_only` and never commits. Invalid requests retain `stale_game`, `stale_turn`, `not_ready`, `illegal_column` or `malformed` responses. Malformed lines without a usable ID report ID zero. `status` returns `not_accepted`; serial human moves are never accepted in this version.

Only physical IR passages cause normal human or robot commitment. The Pi never sends human move requests; queued operator commands are discarded on disconnect. Snapshots poll every two seconds and reconstruct committed history without replaying commands.

Additional phase names: `StartupPositioning`, `HumanBaseline`, `HumanConfirm`, `IndexerReset`. Existing field names and protocol version remain unchanged. `pending_column` represents a robot target only; a detected but uncommitted human column remains internal to the ESP32 controller.

`diagnose` emits the cached first PCA failure as a `message` and requests a snapshot. It does not touch outputs, clear a fault or retry initialization. `snapshot` while Fault also repeats that diagnostic message. Diagnostic text identifies the failed step, address, SDA/SCL pins and numeric error/observed value. Shutdown failures do not replace the original cause.

Pi suppresses identical PCA detail text after its first display on a connection. Reconnect, a changed boot ID or explicit `diagnose` allows the report to display again. Different diagnostic text always displays. Firmware snapshots remain periodic and authoritative.

Setup/operator commands: `confirm-clear`, `free`, `coach` (unavailable), `easy`, `medium`, `hard`, `0`/`1` at starter selection, `help`, `board`, `stop`, `restart`, `correct`, `arm-manual`, and `confirm-correction`. ESP32 rejects commands inappropriate to its phase. Pi/API failures cannot clear faults or bypass robot sensor confirmation.

## Other frames

- `message`: `text`, JSON-escaped operator information.
- `search`: `column`, `duration_ms`, `nodes`; diagnostic only, never a committed move.
- `command_result`: `request_id`, `status`.

No provider, coaching or speech service runs in the current Pi application. API keys cannot enable calls.
