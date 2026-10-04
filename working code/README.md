# Connect Four: full game with automatic IR moves

ESP32 owns the board, minimax engine, three buttons, SSD1331 OLED, seven IR sensors, PCA9685 motors, faults and move commitment. The optional Raspberry Pi provides a USB terminal and deduplicated game logs. Normal startup runs no coaching, Gemini, ElevenLabs or speech service, even if the Pi's existing configuration contains API keys.

## Play

1. Clear the physical board and feed path before powering the machine or starting a new game. Healthy startup moves all seven doors up/open and motor 7 to load 110, then opens Mode. Position signals remain active; there is no initial typed clearing command.
2. Left/Right browse Mode; Centre confirms Free Play. Coach is visible with **Unavailable** and cannot be selected.
3. Select Easy, Medium or Hard, then Human or Robot first. Defaults: Easy and Human first. O always starts. Search depths are 2/4/5 plies.
4. Wait for **Insert ONE disc**. All doors are open and motor 7 is loaded. Insert one human disc; its IR sensor registers the column automatically. Do not type a column or insert another disc before the next human prompt.
5. The engine selects the robot column. All other doors close, the target stays open, and motor 7 releases at 180 only after settling and a clear IR baseline. One target passage, stable clear and completed door closure are required before commitment. Motor 7 returns to load 110 before the next human turn.
6. At a win/draw all doors close and motor 7 stays loaded. Use `restart`, physically clear the board/indexer/feed path, then `confirm-clear` to reset and position the machine again.

Buttons connect GPIO13/14/23 (Left/Right/Centre) to GND. They debounce for 30 ms, do not repeat while held and require release between screens. Simultaneous presses do not confirm. Centre operates setup menus; clearing and recovery use the terminal.

OLED shows the Mode → Difficulty → Who starts? menus, then plain turn/status text. It does not draw the game board. The Pi terminal still displays the board.

Terminal setup remains available: `free`, then `easy`/`medium`/`hard`, then `0` for human first or `1` for robot first. Typed human columns are rejected. `coach` reports unavailable. `board`, `snapshot`, `help`, `stop` and recovery commands remain available.

## Stop and recovery

`stop` immediately disables motor signals and IR capture, retaining board/history and any pending robot target. `quit` exits the Pi terminal without stopping ESP32; use `stop` first when movement must stop.

For a PCA fault, type `diagnose` in the Pi terminal. Firmware reports the first failed step, address, SDA/SCL pins, error code and observed register/channel value. The same detail is repeated with Fault snapshots, including after USB reconnect. Diagnosis reads the cached error only; it does not move motors, clear the fault or retry delivery. The Pi startup command opens USB and can encounter an existing fault or a reset-time fault; its timing alone does not identify the cause.

Repeated identical PCA detail messages are displayed once per connection; `diagnose` explicitly shows the report again. ESP32 PCA initialization and primary register reads now follow the working motor-command program: default Wire clock, Adafruit setup, and repeated-start one-byte reads. A failed read gets one checked STOP-separated fallback; empty reads still latch a fault. Firmware announces build 017; the Pi reports a missing snapshot after six seconds.

Wrong/extra/premature robot passages, ambiguous human passages, full-column human input, stuck/noisy sensors and queue overflow pause with outputs disabled. There is no automatic disc retry. For a pending robot move, secure the mechanism and use `correct`, `arm-manual`, deliver exactly one target disc when prompted, then `confirm-correction` after checking physical agreement. No indexer release is issued during correction; after commitment normal load positioning resumes. Human ambiguity or uncertain feed state requires restart and physical clearing. PCA faults require repair and ESP32 reset.

## Install and verify

[DEPLOY.md](DEPLOY.md) contains exact Windows transfer and Pi installation/flashing commands. [WIRING.md](WIRING.md) contains the complete pin and angle table. [Firmware guide](firmware/ConnectFourGameTest/README.md) explains timing and physical checks. [PROTOCOL.md](PROTOCOL.md) defines serial frames.

From this directory run `cmd /c build-native.cmd` for five firmware suites. From `pi`, run `python -m unittest discover -s tests -v`. Run `compile-firmware.ps1`, then `package-deployment.ps1` after checks pass. The full hardware build uses MENU_TEST_ONLY=0. Outputs remain in this directory's `build` and `deployment` folders; the Pi ZIP excludes credentials, environments and logs.

Start the installed terminal with `.venv/bin/python -m connect4`. `--local-only` and `--no-speech` remain compatible no-op flags. No audio setup or API keys are needed. The terminal is a foreground program, not an automatic system service.

## Logs and reconnect

Snapshots and committed moves are validated and logged in `pi/logs/game-<boot_id>-<game_id>.jsonl`. Pending targets are not committed moves. ESP32 stores up to 42 moves in RAM. USB reconnect reconstructs missed committed history without resending moves. Some serial adapters reset ESP32 on open; a reset starts a new empty logical game and automatically positions motors. Clear the physical board/feed path before starting again.

`.venv/bin/python -m connect4 --replay logs/game-42-1.jsonl` prints saved final state without serial, coaching or network calls. No hints, feedback or game review are generated in this version.

Continuous servo holding requires suitable external power. Test unloaded startup, doors, buttons, all seven sensors and stop/fault behavior before loaded play. Passing software checks does not establish final disc seating, loaded retention or one-disc magazine isolation.
