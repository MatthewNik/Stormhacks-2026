# StormHacks 2026: official Connect Four project plan

Status: approved design and future implementation plan, not implemented physical firmware.

## Authority and scope

This document and its companion wiring/calibration and test checklists are the current single-ESP32 design. Read this plan before the older report. The ESP32 runs game rules, AI, physical control and UI through Arduino IDE. Human discs are yellow; robot discs are red. There is no Raspberry Pi, camera, ToF sensor multiplexer, motorized board dump, or network dependency in this version.

The existing Serial Monitor sketch has a working software interface and recorded compile/native-test results. No physical sensor, motor, power, or full-game qualification has been established. This preparation package does not modify firmware. The old physical-controller code is historical reference: its Pi protocol, VL53L1X sensors, SSD1306 driver, and single magazine plus dump channel do not match this plan.

## Confirmed hardware and mechanics

- One ESP32-WROOM development board; select ESP32 Dev Module in Arduino IDE.
- Seven TCRT5000 reflective sensor modules, one below each hatch, looking at a nearby disc face. These are not opposed break-beam pairs.
- One 128 x 64, 1.3-inch SH1106 I2C OLED; monochrome regardless of white/blue seller variant.
- One PCA9685 and nine servo outputs: seven column hatches and two magazine stops. MG996R is the intended servo type; verify actual units.
- A sloped rail carries robot discs over closed hatches to the one open hatch. The downstream rail must retain a disc rather than letting it leave the machine.
- A ramp magazine holds upright discs side by side. Following travel direction: stack, upstream stop, one-disc pocket, downstream stop, delivery rail.
- Board emptying uses a manual mechanical release. Three buttons provide difficulty, first-player selection, and Start/Confirm.
- Bench supply for testing; batteries and the final power system are handled by the power teammate. Positive/negative connections are interface labels, not authorization to connect an arbitrary battery directly.

## Responsibilities and implementation boundary

| Subsystem | Responsibility | Must not do |
| --- | --- | --- |
| Game controller | Own authoritative board, turn, result and pending robot move | Commit from servo commands alone |
| AI | Search copies of a valid board and select a legal column | Operate hardware or mutate the real board |
| Sensor service | Capture edges, qualify passages, report column and time | Guess piece colour or final row |
| Actuator controller | Execute calibrated hatch and two-stop sequence | Blindly retry a release |
| UI | Menu, status, board reference and explicit recovery confirmation | Bypass physical-state checks |

The current engine interface is `chooseMove(board, difficulty, column, stats)`. Inputs are a Board and difficulty; outputs are success, a zero-based column and search time/node statistics. Board stores `cells[6][7]`, seven column heights, move count, winner and `redTurn`. Cells are 0 empty, 1 yellow, 2 red. Row 0 is top; row 5 is bottom. The next row is `5 - height[column]`. User-facing columns are 1-7; internal columns/channels are 0-6.

Minimax tries legal columns, alternates red maximization and yellow minimization, and uses alpha-beta pruning. The local evaluation gives large positive/negative win scores, scores unopposed four-cell windows and centre occupancy. Easy searches 2 plies, medium 4, hard 5. A ply is one player's move. Hard is limited-depth, not guaranteed perfect. Measure actual ESP32 search time at all difficulties; do not assume it is instantaneous.

## Proposed software interfaces

These are implementation contracts, not existing code. A qualified passage event contains column, detection time and clear time. A physical-delivery request contains selected column and a unique local move identifier. Delivery outcomes distinguish confirmed passage, missing passage, wrong column, extra passage, and actuator/bus fault. The game commits each accepted move at most once.

Capture short sensor changes while search runs. Interrupt handlers only capture minimal edges/timestamps; they do not run I2C, draw text, search, or move servos. Use a bounded event queue; overflow faults because disc history is then uncertain. Drain/check pending events before starting robot motion. Use monotonic, rollover-safe elapsed-time checks for phases; no long blocking delays. Freeze the real board during search, delivery and recovery; simulated engine moves use copies.

## Normal state flow

| State | Event / required evidence | Action and next state |
| --- | --- | --- |
| BOOT / SETUP | Logic initialized, menu selected, operator confirms empty board | Initialize matrix; choose first turn |
| HUMAN_PREPARE | Magazine held; calibrated staggered opening completes | Open non-full columns, then display Your turn |
| HUMAN_WAIT | One qualified legal-column passage | Record yellow, check result, then close hatches if play continues |
| ROBOT_THINK | Robot turn, board valid, no unresolved sensor event | Search; retain selected move as pending |
| ROBOT_PREPARE | All hatches commanded closed and calibrated settling elapsed | Open target only; wait gate preparation interval |
| ROBOT_FEED | Target path ready, both magazine stops holding | Run upstream admit/close, downstream release/close sequence |
| ROBOT_CONFIRM | Exactly one target passage, completed gate/feed cycle, settling elapsed | Record red once; check result |
| GAME_OVER | Win or 42-move draw | Display result; retain matrix until manual empty/reset |
| PAUSED | Unexpected or missing passage, stuck signal, queue/bus/actuator failure | Stop automatic feed; freeze board and pending move; offer supervised recovery |

Sensor service remains active in every phase. A passage in an unarmed phase is not silently discarded. During manual board emptying, events are diagnostic only and cannot commit moves. Sensor-clear startup checks are a plausibility check, not proof that wires are connected or the board is empty.

## Human turn and hatch policy

Hold both magazine stops closed. Open only columns with height below six, one servo at a time with measured separation; show Your turn only when the opening sequence has finished. A qualified passage transitions from clear to detected to clear, survives measured noise filtering, occurs in a playable column, and is the only accepted passage for that turn.

Record the human move using the turn colour and known height; increment height and move count, check four-in-a-row/draw. The sensor does not read the row or colour. A second human disc during the transition or robot turn pauses the machine. Close hatches only after the passage clears plus calibrated clearance margin; this reduces interference with the falling disc but cannot prove the chute is empty. Qualify the geometry physically.

## Two-stop robot delivery

1. Keep both stops closed; close all seven hatches with staggered movement.
2. Open target hatch; wait its measured preparation interval.
3. Keep downstream stop closed. Open upstream stop to admit one disc into the pocket.
4. Close upstream stop to hold the stack; wait for its calibrated closing interval.
5. Open downstream stop to release the isolated disc onto the rail.
6. Close downstream stop after a calibrated interval long enough for the disc to clear.
7. Observe target sensor throughout feeding/travel. Latch an early valid passage; do not wait until after servo motion to begin sensing.
8. Require exactly one target passage before timeout, no other column passage, completed actuator cycle and settling interval. Close target hatch when clear; commit red exactly once.

Keep the upstream stop closed throughout downstream release. Stop spacing must mechanically isolate one disc at minimum and maximum fill. Without a pocket sensor or servo feedback, timing is an assumption until measured: a servo command cannot confirm that the stop moved or the pocket filled. A missing passage does not identify an empty magazine versus a jam.

## Pause and supervised recovery

On fault, prohibit additional feed commands. Use a phase-aware response: do not blindly close a stop through a trapped disc, toggle output-enable, or sweep gates as a recovery action. The operator secures the mechanism and checks for trapped discs before requested recovery movements. The power teammate supplies an accessible actuator cutoff. Disabling PWM is not proof of mechanical closure or stack retention.

| Fault | Stored-board rule | Operator recovery |
| --- | --- | --- |
| Extra human disc | Retain last committed board | Remove extra disc, compare every column with OLED board reference, explicitly confirm correction |
| Missing robot passage | Keep chosen red move pending and uncommitted | Locate original disc, clear feed path, explicitly arm manual completion, deliver one red disc through target sensor |
| Wrong robot column | Do not commit misplaced red disc | Remove misplaced disc; explicitly arm manual completion through intended column |
| Extra robot disc or ambiguous events | Freeze all updates | Correct only if every disc is accounted for; otherwise empty and restart |
| Stuck signal, bus failure or lost edge history | No inferred move | Repair and verify detection before resuming; restart if physical/memory agreement cannot be established |
| ESP32 reset/power loss | Do not resume old game | Secure/clear feed path, manually empty board, confirm new game |

Recovery correction mode logs events but does not count discs being removed or moved. Only after a deliberate Arm manual completion action may one expected target passage commit the pending robot move. Any other passage faults again. There is no automatic or menu-driven second magazine release for a missing robot disc. After recovery, both stops must hold, the route must be clear, and the physical board must match the stored board. If that cannot be verified, restart.

## Buttons and OLED

| Context | Difficulty button | First-player button | Start/Confirm button |
| --- | --- | --- | --- |
| Setup | Cycle Easy / Medium / Hard | Toggle Human / Robot first | Confirm settings and empty board |
| Playing | No settings change | No settings change | Hold 2 seconds to request restart |
| Pause | Toggle fault text / stored board | Cycle applicable recovery actions | Confirm selected action; hold requests restart |
| Game over | Read-only | Read-only | Request new game and empty-board confirmation |

A restart request freezes automatic play and opens an explicit Clear board and feed path confirmation screen; it does not immediately zero the matrix. A fresh button press after emptying starts a new setup. For manual recovery, separate Begin correction, Arm manual completion and Confirm correction actions as applicable. Debounce buttons and trigger once per press; require release before confirmations. Determine debounce duration on hardware.

OLED shows difficulty/first player, Your turn, Thinking, Feeding column N, fault/recovery instructions and winner/draw. Board reference uses outlined circles for yellow and filled circles for red with a legend; place a 7 x 6 grid in about 70 x 48 pixels, leaving space for a short caption. It is a memory reference, not a sensed image. For an extra-human correction, show both the committed matrix and detected event column(s).

## Integration order and acceptance

1. Re-run existing Serial Monitor game on ESP32 and record search timing.
2. Bring up 3.3 V logic, OLED, buttons and I2C addresses with servo rail disconnected.
3. Qualify one reflective sensor in the real chute with both colours.
4. Verify seven-sensor column mapping, isolation and capture during AI search.
5. Calibrate unloaded hatches and magazine stops; label channel and endpoints.
6. Validate pocket isolation at low/medium/full magazine fill.
7. Integrate one target delivery, timeout and supervised recovery.
8. Integrate all columns, human transitions, UI and full games.
9. Complete fault injection and demo rehearsal using TEST_CHECKLIST.md.

Do not progress past a failing stage. Calibration values remain blank until measured. The software lead owns the state machine, logs, matrix and integration. The mechanical teammate owns reliable routing, one-disc isolation and removal access. The power teammate owns supply voltage/current, battery conversion, distribution, fuse/cutoff and current/rail measurements. Agree that a commanded endpoint and elapsed timer are not position feedback.

## Reference directory

Sources checked 2026-10-01. Official sources support component/library behavior; seller descriptions support only the intended purchased module identity. Exact physical-module compatibility still requires measurement.

| Item | Repository / source | Documentation / examples |
| --- | --- | --- |
| Minimax | [GitHub](https://github.com/ripred/Minimax) | [Engine API and examples](https://github.com/ripred/Minimax#readme) |
| Adafruit SH110X | [GitHub](https://github.com/adafruit/Adafruit_SH110x) | [API](https://adafruit.github.io/Adafruit_SH110x/html/index.html), [examples](https://github.com/adafruit/Adafruit_SH110x/tree/master/examples) |
| Adafruit GFX Library | [GitHub](https://github.com/adafruit/Adafruit-GFX-Library) | [API](https://adafruit.github.io/Adafruit-GFX-Library/html/index.html), [guide](https://learn.adafruit.com/adafruit-gfx-graphics-library) |
| Adafruit PWM Servo Driver Library | [GitHub](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library) | [API](https://adafruit.github.io/Adafruit-PWM-Servo-Driver-Library/html/index.html), [examples](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library/tree/master/examples) |
| Adafruit BusIO | [GitHub](https://github.com/adafruit/Adafruit_BusIO) | [API](https://adafruit.github.io/Adafruit_BusIO/html/index.html) |
| ESP32 Arduino core | [GitHub](https://github.com/espressif/arduino-esp32) | [Docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/), [installation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html), [GPIO](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html), [I2C](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html) |
| PCA9685 board | [Wiring guide](https://learn.adafruit.com/16-channel-pwm-servo-driver/hooking-it-up) | [Library guide](https://learn.adafruit.com/16-channel-pwm-servo-driver/using-the-adafruit-library) |
| TCRT5000 sensor element | [Vishay datasheet](https://www.vishay.com/docs/83760/tcrt5000.pdf) | Module comparator/pull-up schematic supplied by user; verify actual board |
| MG996R | [TowerPro specifications](https://towerpro.com.tw/product/mg996R/) | Calibrate actual servo units |
| OLED seller listing | [Hosyond SH1106](https://www.amazon.ca/dp/B0C3L7N917) | User supplied photos/specifications; listing was inaccessible to web verification |
| Sensor seller listing | [User-supplied Amazon link](https://www.amazon.ca/dp/B00XT0PBC0) | DAOKI TCRT5000 description/photos supplied by user; ASIN identity not independently verified |

## Arduino setup

Boards Manager: install esp32 by Espressif Systems; select ESP32 Dev Module. COM8 was previously identified as this board's CP210x USB port; check current assignment. Existing serial test uses 115200 baud and Newline or Both NL and CR. The handoff records a successful compile using ESP32 core 3.3.11, but not a physical upload/play test.

Library Manager: install Adafruit SH110X, Adafruit GFX Library, Adafruit PWM Servo Driver Library and Adafruit BusIO. Accept Install all dependencies and verify they are present; automatic dependency installation depends on the IDE prompt. Record installed versions before integration. Use the SH1106 driver, not the old SSD1306 driver. Arduino core supplies Wire, GPIO and timing support. Sensors/buttons require no additional library. Servos connect to PCA9685, so no separate ESP32 direct-servo library is required. Minimax.h version 1.0.0 and its license are bundled beside the active sketch; no separate installation is needed.
