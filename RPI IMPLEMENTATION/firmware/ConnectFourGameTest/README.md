# ESP32 Connect Four with Pi telemetry and three-button setup

Open this directory's `ConnectFourGameTest.ino` for the integration version. The original active-workspace sketch remains separate. ESP32 Dev Module owns the authoritative board, AI, buttons, 96x64 SSD1331 OLED, sensors, PCA outputs, faults and commitment. Raspberry Pi connects over USB serial at 115200 baud; no network is required by ESP32.

See the [integration guide](../../README.md) for menu controls, terminal commands, API configuration and Pi setup, and the [protocol reference](../../PROTOCOL.md) for structured telemetry and tagged human moves. Startup requires `confirm-clear`, then mode selection. Free Play selects difficulty before starter. Coach starts human-first with medium AI and an adaptive centre-opening lesson. Difficulty depths remain 2/4/5 plies; hard is not guaranteed perfect.

## Commitment and recovery

Human moves are typed columns 1–7, committed by gravity only in `HumanReady`. Human-turn IR capture is disabled. Robot delivery closes hatches, computes a local move, opens the pending target, checks stable-clear sensors, loads PCA7 at 90°, settles 300 ms, waits 500 ms, releases at 0° and settles 300 ms. The pending target does not change the committed board.

Exactly one expected IR passage starting after release must qualify, followed by 100 ms quiet time and completed hatch closure. A timer alone never commits a robot move. Wrong/extra/premature/missing/stuck/noisy/overflowed sensor history pauses with the board and pending move retained, outputs disabled, and no automatic feed retry.

From pause/stop, `correct` enters supervised correction. Secure the mechanism, then `arm-manual` checks sensor baselines and arms one manual target passage. PCA7 is never commanded during manual completion. After the passage and closure, `confirm-correction` acknowledges physical agreement before exactly-once commitment. Uncertain physical board/feed state requires clearing and restart. `restart` retains board/history until `confirm-clear`. PCA bus/output faults require repair and ESP32 reset; recovery commands do not clear them.

## Wiring and calibration

Disconnect USB and servo power before changing wiring. Preserve common ground, separate PCA logic VCC and servo V+, and the external 10 kOhm OE pull-up to 3.3 V.

| Function | Connection |
| --- | --- |
| PCA SDA / SCL | GPIO21 / GPIO22; address 0x40 |
| PCA OE | GPIO25, active LOW; external pull-up |
| Hatches, columns 1–7 | PCA0–6; PCA2–4 remain isolated |
| Indexer | PCA7; load 90°, release 0° |
| Unused outputs | PCA8–15 stay FULL_OFF |
| IR columns 1–7 | GPIO34, 35, 36, 39, 32, 33, 27 |
| OLED clock / data | GPIO18 / GPIO19 SPI; no MISO |
| OLED reset / DC / CS | GPIO16 / GPIO17 / GPIO26 |
| OLED VCC / PCA logic VCC | 3.3 V |
| Left / Right / Centre buttons | GPIO13 / GPIO14 / GPIO23 to GND; internal pull-ups |

IR modules must be 3.3 V-powered with 3.3 V-safe DO; AO is unused. GPIO34–39 require external/module pull-ups. OLED SCL/SDA labels refer to SPI, not PCA I2C. `GameConfig.h` owns pin assignments and calibration.

Sensors use a 64-entry interrupt queue, 2 ms continuous detection and 20 ms continuous clear qualification, and 100 ms stable-clear baseline/quiet time. Baseline deadline is 2000 ms; stuck detection 1000 ms; passage deadline 3000 ms. Capture resets discard stale human/menu/correction events. Queue overflow, backlog and continuously noisy waits are bounded. Clear sensors do not prove wiring connectivity, final seating or an empty feed path.

Servo pulses remain 50 Hz with nominal 500–2500 us endpoints. The shared scheduler owns OE and pulse expiry for PCA0–7. Ordinary pulses expire after 300 ms; selected robot hatch pulses after 1600 ms. Hatch commands do not reset indexer deadlines. Stop/restart/fault clears outputs so stale pulses cannot return.

## Physical limits

This remains an unassembled bench test. Printed PCA connectors 2–4 were reported to spin through full revolutions and remain disabled, corresponding to columns 3–5. Software selections and IR bench confirmation are allowed there, but no hatch position pulses are emitted. OLED marks an isolated pending target with `!`.

Pulse expiry removes holding torque. The 3000 ms sensor deadline outlasts the 1600 ms selected-hatch drive window. This does not guarantee loaded-hatch retention, one-disc isolation or safe stack holding. Software timing is not an independent power cutoff if ESP32 hangs. Start servo power at 4.8 V, remain within the specified 4.8–6.0 V range, use an accessible supply cutoff, and never route servo current through ESP32 or breadboard logic rails.

## Verification and hardware checks

Dependencies: ESP32 core, Adafruit PWM Servo Driver, GFX and BusIO. The bundled SSD1331 driver and its upstream license must stay beside the sketch. Use ESP32 Dev Module and Verify; uploading is manual.

From a Visual Studio developer PowerShell, run `tests/run-native.ps1`. Four native suites cover the existing gameplay/output/sensor/recovery behavior plus three-button wraparound menus, centre confirmation, debounce/held/chord/release handling, tagged move rejection, request deduplication, current-game history, telemetry overflow/escaping and coach opening. Stub tests do not reproduce hardware interrupt latency or electrical behavior.

With servo power off, verify clear/mode/difficulty/starter screens and all three buttons, including wraparound, no repeat while held, chord suppression and release between screens. Centre must not acknowledge clearing or recovery. Then perform unloaded checks of all seven sensors, indexer endpoints, clear baselines, correct/extra/wrong/missing passages, manual correction without PCA7 release, and stop/restart/fault behavior. Test Pi USB disconnect and API/audio failure; neither may invent moves or trigger another release. Identify and calibrate the spinning servos before re-enabling PCA2–4. Establish mechanical retention and one-disc isolation before any loaded run.

No integrated upload, real chip release, sensor timing qualification or loaded-mechanism validation is established by software checks.
