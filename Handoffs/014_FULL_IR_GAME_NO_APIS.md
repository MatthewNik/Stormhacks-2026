# Full IR game with unavailable Coach option

## Current version

Deploy from [working code](../working%20code/README.md). [DEPLOY.md](../working%20code/DEPLOY.md) has exact Windows transfer, fresh Pi dependency setup, ESP32 flashing and terminal startup commands. The firmware and Pi packages in `working code/deployment` were regenerated and verified. Other saved programs were not updated.

ESP32 remains authoritative for the board, minimax, buttons, OLED, sensors, motors, faults and move commitment. Pi provides USB operator commands, snapshots, validated game logs and local replay. The running application constructs no provider, coaching or speech service, including when existing configuration contains API keys. No hints, feedback, speech or automatic game review run. `--local-only` and `--no-speech` remain compatibility no-ops. Dormant provider/coaching modules remain available for future development but are not imported by the running application.

## Startup, controls and motor holding

Healthy boot initializes an empty logical game and positions PCA0–6 up/open at **115/110/100/105/100/115/110°**, followed by PCA7 **110° load**. Starts are at least 50 ms apart, and the final command settles for 300 ms before Mode. Startup emits no release command. Position signals remain active throughout menus and play; motor outputs no longer expire after short travel pulses. Stop/restart/fault disables every signal and OE. External servo power remains separate from PCA logic power.

Left/Right browse Mode, difficulty and starter; Centre confirms Free Play and its setup. Coach stays visible on OLED with **Unavailable** and cannot be selected by buttons or terminal. Existing debounce, held-button suppression, chords and release-between-screens behavior remain. Defaults: Free Play, Easy, Human first. Search depths remain 2/4/5, and O starts.

## Physical turn flow

- Human: all seven doors open, including full columns, and PCA7 holds load. Sensors arm after door settling and require 100 ms stable clear before **Insert ONE disc**. One qualified legal-column passage and atomic stable-clear sealing commit exactly one move. Typed columns are rejected. Human waiting has no turn timeout. Full-column or ambiguous/extra input pauses; recover by physically clearing and restarting.
- Robot: the engine chooses first. A staggered batch holds the target open and closes the other six doors. After settling and clear baseline, PCA7 holds load for 300+500 ms then releases at **180°**. One expected target passage, quiet time, completed door closure and atomic sealing precede commitment. PCA7 returns to load and waits 800 ms before the next human opening or game-end display.
- Game end: all doors close and PCA7 holds load. Restart retains history until explicit physical clearing and `confirm-clear`, then repeats startup positioning.

Wrong/extra/premature/stuck/noisy/overflowing robot sensing and unsettled/ambiguous human sensing disable outputs and pause without retrying a release. Activity arriving immediately after human commitment is checked before robot search/feed. Pending robot correction still uses `correct`, `arm-manual`, one target passage and `confirm-correction`; it issues no indexer release. After commitment normal load positioning resumes. PCA faults require repair and ESP32 reset.

## Interface and deployment

Protocol remains v1 with its existing fields. New phases: `StartupPositioning`, `HumanBaseline`, `HumanConfirm`, `IndexerReset`. `pending_column` is a robot target only. Valid tagged serial human requests return `sensor_only` without commitment; other validation responses remain. Pi never sends human move requests or replays commands after reconnect. Serial terminal starter selections 0/1 remain available on FirstPlayer.

Pi user/path defaults remain `matthew` and `~/stormhacks-rpi`; CP2102 path is `/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0`. Preserve remote `.env`, environments, logs and Bluetooth settings. The fresh Pi needs Python virtual-environment support and esptool for flashing; audio/API setup is unnecessary. Keep servo power off during flashing. Clear the physical board/feed path before play. Serial reopening may reset ESP32, automatically commanding startup positions and clearing RAM history.

## Verification

All five firmware suites and all 21 Pi tests passed. Coverage includes all seven human columns, holding/startup angles and staggering, unavailable Coach, three-button menus, bounce/extra/overlapping/full-column IR, atomic seal races, target-only opening, one robot release per turn, reload, stop/fault/restart, manual correction, telemetry and reconnect without replay. Complete simulated games passed for both starters at every difficulty with board/history replay and release-count checks. Existing dormant provider/worker tests use mocks; no live API calls were made.

Final full ESP32 compilation passed: **339504 bytes flash / 33820 bytes RAM**. Packaged merged firmware exactly matches the final build and includes the expected sensor-only and boundary-guard behavior markers. Both SHA256 entries and ZIP integrity passed; every packaged Pi file matches current source, with no credentials, environments or logs included.

No remote upload, physical movement, actual disc feeding or loaded-mechanism qualification was performed. Verify unloaded startup/holding, all buttons and seven sensors, then mechanical retention, supply capacity and one-disc magazine isolation before loaded play.
