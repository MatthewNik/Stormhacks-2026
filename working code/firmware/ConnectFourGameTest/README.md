# ESP32 full IR Connect Four controller

ESP32 Dev Module owns the board, minimax engine, PCA9685 outputs, IR capture, buttons, SSD1331 OLED and commitment. Pi only observes/logs states and sends operator commands. See [game guide](../../README.md), [wiring and calibration](../../WIRING.md), [protocol](../../PROTOCOL.md) and [installation commands](../../DEPLOY.md).

## Startup and menus

Healthy boot initializes an empty logical game, enters StartupPositioning, commands PCA0–6 open and PCA7 load 110 at least 50 ms apart, and waits 300 ms after the final command before Mode. Motor signals remain active to hold positions. Startup and menu navigation cannot release a disc. Clear the physical board/feed path before starting. Restart retains stored history until `confirm-clear`, which clears logical state and repeats positioning.

Left/Right browse; Centre confirms Free Play, difficulty, then starter. Coach is visible but Unavailable and cannot be selected by buttons or serial. Defaults are Easy/Human; depths are 2/4/5. O starts. Buttons use 30 ms debounce, no held repeat, chord suppression and release between screens. Centre does not acknowledge clearing or recovery.

The OLED renders setup menus and plain turn/status text, without a board grid or disc graphics. Board snapshots remain visible in the Pi terminal.

## Turn sequencing

HumanOpening opens all seven doors, including full columns, with staggered commands. HumanBaseline arms IR and requires 100 ms stable clear within 2000 ms before HumanReady displays Insert ONE disc. One qualified legal-column detection enters HumanConfirm. Passage and stable-clear atomic sealing precede a single board/history update. Typed columns and tagged serial moves cannot commit. HumanReady has no turn timeout. Full-column detections, extra passages, stuck/overflowing capture or an unsettled HumanConfirm pause without commitment.

RobotSearch uses the local engine while doors remain open. RobotOpening commands all non-target doors closed and the target open, waits for the batch to settle, then RobotBaseline requires stable clear. PCA7 holds load 110 for 300 ms settling plus 500 ms loading, then moves to release 180. Exactly one target passage starting after the release command must qualify. After stable clear, all doors close. Atomic sealing after closure precedes robot commitment. IndexerReset commands load 110 and waits 800 ms before opening doors for the human or reporting game end. Human wins/draws close all doors and keep the indexer loaded without another release.

IR qualification: 2 ms continuous active LOW, 20 ms clear, 100 ms stable clear, 1000 ms stuck deadline, 3000 ms passage deadline. A 64-entry ISR queue records edges; overflows/backlog pause. Premature/wrong/extra robot discs pause; no timer or automatic retry commits a move. Capture epochs prevent stale edges from becoming new moves. Clear values alone do not prove connectivity, seating or an empty board.

## Outputs and recovery

Every seven-door batch commands PCA0 through PCA6 at least 50 ms apart; delayed loops do not catch up in a burst. Startup adds PCA7 last. Normal indexer commands are separately gated by the delivery state. PCA8–15 remain FULL_OFF. PWM is 50 Hz with 500–2500 us endpoints. Positions hold until changed; stop/restart/fault disables OE and every channel. External servo power remains separate from logic power.

`stop` retains the logical board and pending robot move but disarms sensors and disables motors. `restart` requires physical clearing and `confirm-clear`. Fault requires repair and ESP32 reset. For a pending robot move, `correct`, secure the indexer/feed path, `arm-manual`, one target passage when prompted, then `confirm-correction` acknowledges physical agreement. No PCA7 release occurs in correction; after commitment normal load positioning resumes. Human ambiguity requires physical clearing/restart.

PCA diagnostics preserve the first failed step across shutdown writes: I2C initialization/address probe, Adafruit initialization, register selection/read, invalid PWM frequency, changed prescaler, sleep mode, FULL_OFF writes or motor position writes. The message includes address/pins and numeric code/observed value. `diagnose` emits the cached report; Fault snapshots repeat it so a reconnect does not lose the original cause. It does not probe/reconfigure hardware, resume play or release a disc. All original fault checks remain enabled.

PCA initializes before the OLED and sensors with the same default-clock `Wire.begin(SDA,SCL)`, 50 ms timeout, Adafruit initialization, oscillator setting, 50 Hz setup and prescaler-aware pulse conversion as MotorCommandTest. Primary reads use that program's `endTransmission(false)` and `requestFrom(address,uint8_t(1))` sequence. If unsuccessful, one checked STOP-separated read follows. Both paths require an actual byte; no prescaler or mode value is guessed. Frequency, awake-mode, runtime-prescaler and output-write checks remain required. The UART uses a 512-byte transmit buffer and announces build 017.

Calibration: PCA0–6 closed 15/8/11/12/11/12/12°, open 115/110/100/105/100/115/110°; PCA7 load 110°, release 180°. Column 1 is front-right/PCA0; column 7 front-left/PCA6. IR GPIOs 34/35/36/39/32/33/27 follow that order. Full pin/power/button/OLED details are in the wiring guide.

## Verification

Dependencies: ESP32 core, Adafruit PWM Servo Driver, GFX and BusIO, plus the bundled SSD1331 driver/license. The compile script enforces MENU_TEST_ONLY=0. Run `tests/run-native.ps1` from a Visual Studio developer shell, or the integration directory's `build-native.cmd`, for five native suites. All outputs stay in the active workspace.

Use an external servo supply within the motors' ratings and an accessible power cutoff. Continuous holding must be tested for supply capacity, temperature and loaded retention. Software is not an independent power cutoff if ESP32 hangs. Qualify unloaded angles, every IR input, all buttons, extra/wrong/missing discs, stop/fault and recovery before loaded play. No software test establishes final seating or one-disc isolation.
