# Connect Four: typed human moves, robot IR and indexer

ESP32 Dev Module bench integration with the working **96x64 SSD1331** OLED. Use **Serial Monitor**, **115200 baud**, and Newline, CR or CRLF. Human moves are typed; robot moves require an IR passage in the selected column. ESP32 owns the authoritative board. No Pi or network is required.

This is an **unassembled bench test**, not qualified physical gameplay. PCA connectors **2, 3 and 4 remain isolated** following reported full revolutions. Software may select their game columns **3, 4 and 5** and accept their IR confirmation, but their servo outputs stay FULL_OFF. A pending isolated target is marked `!` beside its column number on OLED. Do not dispense real chips through an unprepared loaded mechanism.

## Start and play

1. Clear the board, indexer and feed path, then enter `confirm-clear`. Startup holds outputs disabled until this operator confirmation and both menu selections.
2. Enter `0` for human first or `1` for robot first, then `easy`, `medium` or `hard`. The starter keeps **O**, the other keeps **X**, for the entire game. Difficulty search depths remain **2/4/5 plies**.
3. Wait for `Human turn: type column 1-7. IR input ignored.` Playable hatches are commanded open, full columns closed. Enter one digit **1-7** to commit the human move using gravity. Human-turn IR activity is not sampled by the interrupt handler or registered as game input.
4. The robot closes all hatches, searches board copies, and saves its chosen column as **pending**. The board stays unchanged during delivery. The OLED shows the committed board, pending column and current delivery phase.
5. Only the selected hatch is commanded open. After settling and a stable-clear sensor check, PCA connector **7** moves to **90°** to load the indexer, settles for **300 ms**, then waits another **500 ms** for loading. It moves to **0°** to release and settles for **300 ms**.
6. Exactly one qualified target IR passage must begin after the successful release command and complete before the confirmation timeout. Early passages during release motion are retained. After a **100 ms** quiet interval, all hatches close and settle while sensing remains active. Only then does the robot symbol commit once.
7. Wins/draws retain the final board with hatches commanded closed. There is no robot delivery after a human win or draw.

Example, waiting for each prompt:

```text
confirm-clear
0
medium
4
```

There is **no automatic one-second commitment** anymore. In an unassembled test, briefly interrupt the expected column's sensor after the release message, then clear it. No passage means a pause rather than an invented board move.

| Command | Meaning |
| --- | --- |
| `help` | Commands and selection syntax |
| `board` | Print the committed ASCII board |
| `stop` | Disable outputs/capture, freeze board and pending move |
| `restart` | Cancel delivery and request clearing confirmation; retain board/pending information |
| `confirm-clear` | After clearing, reset board/pending state and return to player selection |
| `correct` | From pause/stop with a pending robot move, enter supervised correction |
| `arm-manual` | Acknowledge a secured/corrected mechanism; check clear sensors, then arm one manual target passage |
| `confirm-correction` | After manual passage, quiet time and closure, acknowledge physical agreement and commit once |

Commands are phase-specific. Settings cannot change during play. Invalid/full-column moves leave board and turn unchanged. Inputs starting before human readiness are rejected. The bounded parser trims outer spaces/tabs, ignores blank lines, and rejects extra arguments, non-ASCII/control characters and lines over 47 characters.

## Sensor qualification and failure behavior

The robot capture window starts after human hatch closure, before search/opening, and continues through final closure/commit. Baseline sampling does not treat a signal already active as a new passage. Capture is disabled in human phases, menus, game end, clearing screens, stop and unarmed correction. Each capture-mode reset atomically clears the queue and qualification history; old human/correction events cannot confirm a later robot move.

Interrupts capture only column, level and microsecond timestamp into a **64-entry queue**. Main-loop processing qualifies continuous detection **>=2 ms**, followed by continuous clear **>=20 ms**. A shorter clear gap retains the same passage; shorter detection glitches do not create a chip event. These defaults require physical calibration and cannot distinguish touching chips without a clear gap.

All seven sensors must be continuously clear for **100 ms** before indexer loading or manual arming. The clear-baseline deadline is **2000 ms**; a sensor remaining active for **1000 ms** pauses sooner. Automatic/manual passage timeout is **3000 ms** from the release/arming boundary. Quiet and final sealing waits are also bounded so repeated small glitches cannot hang delivery indefinitely.

Premature detection, wrong column, another detection after the accepted one, stuck input, missing passage, overflow or excessive queue backlog pauses delivery. Timestamp comparison uses the original detection start: a LOW beginning before release cannot become valid just because it clears later. Capture remains active through closing; queued events and raw input levels are checked again at the atomic final disarm boundary.

On pause, the committed board and pending move are frozen, outputs are disabled, and the magazine is **never automatically released again**. `correct` and unarmed correction cannot count sensor events. After securing the indexer and correcting the physical position, use `arm-manual`, wait for its prompt, and deliver **one robot chip through the pending target sensor**. Manual completion never commands PCA7 and can recover an interrupted indexer cycle. After closure, `confirm-correction` is required. A conflicting event pauses again; uncertain board/feed state requires clearing and restart.

PCA initialization/read/write/configuration faults disable outputs and require **ESP32 reset** after repair. Recovery commands cannot clear a PCA fault. Sensor-clear checks cannot prove wiring connectivity, final chip seating or an empty feed path.

## Wiring and configuration

Disconnect USB and servo power before changing wiring. Preserve common ground, separate PCA logic VCC and servo V+, and the external **10 kOhm OE pull-up** to 3.3 V.

| Function | ESP32/PCA connection |
| --- | --- |
| PCA I2C SDA / SCL | D21 / D22; address 0x40 |
| PCA OE | D25, active LOW |
| Seven hatches, columns 1-7 | PCA connectors 0-6; connectors 2-4 isolated |
| Indexer | PCA connector 7; load 90°, release 0° |
| Unused actuators | PCA connectors 8-15 stay FULL_OFF |
| IR columns 1-7 | D34, D35, VP/GPIO36, VN/GPIO39, D32, D33, D27 |
| OLED clock / data | D18 / D19 SPI MOSI; no MISO |
| OLED reset / DC / CS | D16 / D17 / D26 |
| OLED VCC and PCA logic VCC | 3.3 V |

IR modules use 3.3 V power and 3.3 V-safe DO, common ground, AO unused. Default detection polarity is active LOW, independently configurable. GPIO34-39 have no internal pull-ups: verify the module output pull-up. Do not connect 5 V DO to ESP32. The OLED SCL/SDA labels refer to SPI, separate from PCA I2C.

`GameConfig.h` owns pins, per-channel polarity, pulse endpoints, hatch enable flags, indexer angles and timings. All servo pulses use **50 Hz**, configured **500-2500 us** endpoints and the actual PCA prescaler for conversion. These are nominal, not measured mechanical calibration.

One shared output scheduler owns OE and deadlines for channels 0-7. Ordinary commands release pulses after **300 ms**; a selected robot hatch has a **1600 ms** window. Hatch batches never reset indexer output/deadlines. Stop/restart/fault disables OE first and clears all eight commands so later enabling cannot resurrect stale motion. Failed pulse cutoffs latch PCA faults.

**Pulse release removes holding torque.** The 3000 ms sensor wait can outlast the hatch's 1600 ms drive window. This is permitted only for the stated unassembled bench test, with no guarantee of holding/routing/stack retention. Timing limits are serviced by the loop, not an independent power cutoff if ESP32 hangs. Use unloaded servos and an accessible supply cutoff. Start servo power at **4.8 V**, stay within the supplied package's 4.8-6.0 V range, and verify wiring/current capacity. Never route servo current through breadboard logic rails or ESP32 supply pins.

## Software verification

Select **ESP32 Dev Module** in Arduino IDE and open `ConnectFourGameTest.ino`. Install **Adafruit PWM Servo Driver Library**, **Adafruit GFX Library**, and **Adafruit BusIO**. SSD1331 **1.3.0** is bundled with its upstream license in `src/ssd1331`; keep that folder alongside the sketch. Use **Verify**; no upload is performed automatically.

From a Visual Studio developer PowerShell at the active workspace root:

```powershell
./Arduino/ConnectFourGameTest/tests/run-native.ps1
```

Three native suites exercise the production game/AI/controller/scheduler, PCA adapter, and ESP32 capture adapter under stubs. Coverage includes both starting players, symbol retention, all difficulty depths, gravity/full/invalid moves, every win direction, draws, AI wins/blocks and immutable boards; human-event suppression and stale history; initial active inputs, noise/chatter, early/wrong/extra/missing passages, stuck signals, overflow/backlog, timer wraparound and quiet-time regressions; PCA7 sequencing/shared OE/expiry, isolated channels, stop/restart/fault interruption, manual completion without re-release, clearing confirmation and exactly-once commitment. Assertions must remain enabled. Stub tests do not reproduce real interrupt latency or multicore contention.

Compilation passed for ESP32 Dev Module: **324256 bytes flash**, **25228 bytes global RAM**, using ESP32 core **3.3.12**, PWM Servo Driver **3.0.3**, SSD1331 **1.3.0**, GFX **1.12.6**, BusIO **1.17.4**. All three native suites passed. Final independent review found no remaining actionable defects after the timing and queue regressions were fixed. Build/test outputs remain under `tmp/connect-four-game-test`.

```powershell
$env:TEMP = "$PWD/tmp/connect-four-game-test/temp"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
& 'C:/Users/matth/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe' compile `
  --jobs 1 --fqbn esp32:esp32:esp32 `
  --config-file tmp/connect-four-game-test/arduino-cli.yaml `
  --libraries 'C:/Users/matth/Documents/Arduino/libraries' `
  --build-path tmp/connect-four-game-test/esp32-ir-build `
  --build-property compiler.cache_core=false Arduino/ConnectFourGameTest
```

## Unloaded hardware checks

1. With servo power off, confirm clearing/selection menus and OE HIGH. Confirm-clear acknowledges actual clearing; it does not sense an empty board. OLED must fit assignments, difficulty, all column numbers, six rows and status/pending target.
2. Validate each IR module's polarity and DO voltage with the separate sensor bench sketch. Confirm a deliberate target interruption produces a complete passage with the selected thresholds. Full-channel clear checks require all seven inputs to be connected and stable.
3. Verify channel7 is a positional servo with free clearance and calibrated load/release travel. Test unloaded indexer sequencing before any magazine discs. Keep connectors2-4 disconnected/disabled until their model and feedback behavior are verified.
4. During the human phase, wave objects past all sensors: board and turn must remain unchanged. Type a legal human move; verify closure, robot selection, selected-hatch command and indexer load/release order.
5. After the release message, interrupt the selected IR sensor, then clear it. Verify the board stays pending through release settling, quiet time and closing, then commits once. Verify no timer alone commits a robot move.
6. Test wrong/extra/pre-release interruptions, no target passage, a stuck input, and noisy inputs: pause must freeze board/pending move without another load/release. Use manual correction with the magazine secured; verify PCA7 never moves in recovery.
7. Stop at each delivery phase, then restart: outputs must stay disabled, old board retained until confirm-clear, and stale events unable to resume delivery. With power off between wiring changes, verify a disconnected PCA produces a reset-only fault. Never induce shorts or disconnect live motor wiring.

No integrated firmware upload, real chip release, sensor timing qualification, servo position measurement or loaded-mechanism validation was performed by these software checks. Physical integration must establish one-disc isolation, gate retention, calibrated timing and sensor-confirmed recovery before loaded operation.
