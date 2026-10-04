# Updated servo defaults and automatic startup menu

## Current calibration

The operator supplied this complete replacement calibration. Motor numbers are printed PCA9685 connector numbers, ordered right to left when viewed from the front. Up is open; down is closed.

| PCA motor | Column | Up/open | Down/closed |
| --- | --- | --- | --- |
| 0 | 1, rightmost | 115° | 15° |
| 1 | 2 | 110° | 8° |
| 2 | 3 | 100° | 11° |
| 3 | 4, centre | 105° | 12° |
| 4 | 5 | 100° | 11° |
| 5 | 6 | 115° | 12° |
| 6 | 7, leftmost | 110° | 12° |

PCA7 magazine indexer: load **110°**, unload/release **180°**. These values supersede the earlier angle tables in handoffs 009–012. Pulse mapping remains 500–2500 us at approximately 50 Hz. IR pins remain 34,35,36,39,32,33,27 for columns 1–7. All seven hatches remain enabled; full-game flap command starts remain at least 50 ms apart.

## Updated programs

- [working code](../working%20code/README.md): full-game `GameConfig.h` now uses these calibrated open/closed and indexer defaults. OLED and serial indexer labels reflect load110/release180. Normal gameplay retains ESP32 motor/IR/state ownership, independent magazine sequencing, fault handling and exactly-once move commitment. Human moves remain entered through the Pi terminal; robot moves require IR confirmation.
- [motor command](../motor%20command/README.md): numeric commands remain available, e.g. `servo 0 100`. Added `servo <0-6> up`, `servo <0-6> down`, `servo 7 load`, and `servo 7 unload`. ESP32 resolves these presets from its calibration; Pi forwards validated names and does not duplicate the angle table. Wrong motor/preset combinations are rejected. Heartbeat/off/fault behavior is unchanged.

The saved button-test and original RPI IMPLEMENTATION directories were not changed. They retain older calibration/startup behavior and should not be treated as the latest full game or motor diagnostic.

## Automatic full-game startup implemented

The pending request in handoff 011 is now implemented. Successful full ESP32 hardware initialization opens Mode automatically, initializes the empty logical game/history and defaults, and leaves motor outputs disabled and sensor capture disarmed. No initial typed `confirm-clear` is required. Starting gameplay still requires button/terminal game selections; startup itself commands no angle or disc release.

PCA initialization failures still latch Fault. Explicit restart and supervised recovery still require clearing confirmation when appropriate; USB reconnect does not replay physical moves. The operator must physically clear board, magazine/indexer and feed path before beginning a new game because the sensors cannot prove an empty physical board. Pi startup text and current deployment/wiring guides were updated accordingly.

## Verification and deployment status

Full-game five native suites and all 19 Pi tests passed, including added healthy automatic-menu startup, no motor movement while browsing, explicit restart clearing and fault-preserving startup checks. The motor native suite passed numeric parsing/pulse/off/fault/heartbeat tests and every calibrated preset, including invalid-role rejection. All five motor Pi tests passed with named preset validation.

Both ESP32 builds passed: full game 338384 bytes flash / 33876 bytes RAM; motor diagnostic 301264 bytes flash / 23764 bytes RAM. Both deployment firmware binaries were regenerated, matched to their final builds, and verified alongside SHA256 manifests and Pi ZIP integrity. The full calibration table was checked against the supplied values. ZIPs exclude credentials.

To apply these changes, transfer the new packages and flash the relevant ESP32 firmware, plus update its matching Pi code using each program's README/DEPLOY instructions. Motor presets require the updated motor terminal as well as firmware. Motor testing still replaces game firmware while installed. No actual Pi upload, ESP32 flash, or physical movement was performed during this update. Loaded retention, one-disc isolation and real IR qualification remain hardware checks.

## Fresh Pi and SSH context

The user reinstalled Raspberry Pi OS Lite on the same SD card, explaining the changed SSH host keys. They subsequently reported SSH working. Earlier Pi virtual environments/API/Bluetooth setup should not be assumed present on the fresh OS. Install dependencies and flashing tool as needed; the motor diagnostic uses no API keys or speech services. Reconnect after adding the Pi user to dialout. Motor files use the separate `~/stormhacks-motor-test` directory.
