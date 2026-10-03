# Wiring and calibration contract

Companion to PROJECT_PLAN.md. These are planned assignments; verify physical board labels before wiring. No loaded motion is authorized by the old firmware's endpoints.

## Point-to-point wiring

| Component / signal | Connection | Check |
| --- | --- | --- |
| ESP32 | USB logic supply during initial tests | ESP32 Dev Module; confirm CP210x port |
| OLED VCC / GND | Clean 3.3 V / common ground | SH1106; four-pin I2C module |
| OLED SDA / SCK | GPIO21 / GPIO22 | SCK is SCL on this module, not SPI |
| PCA9685 VCC / GND | Clean 3.3 V / common ground | VCC is logic only, not motor power |
| PCA9685 SDA / SCL | GPIO21 / GPIO22 | Shared bus; expected PCA address 0x40 |
| PCA9685 OE | GPIO25; external 10 kOhm pull-up to 3.3 V | Active low; disabled during boot initialization |
| TCRT5000 VCC / GND, all seven | Clean 3.3 V / common ground | Verify operation at 3.3 V, actual DO high/low |
| TCRT5000 DO, columns 1-7 | GPIO34,35,36,39,32,33,27 | One input per column, no I2C mux |
| TCRT5000 AO | Unconnected initially | Optional future diagnostic, not game input |
| Buttons: Difficulty / First player / Start | GPIO13 / GPIO14 / GPIO23 to GND | Active low, internal pull-ups, debounced |
| Servo signals: hatches 1-7 | PCA channels 0-6 respectively | Index 0 means physical column 1 |
| Servo signal: upstream stack stop | PCA channel 7 | Upstream is earlier in disc travel |
| Servo signal: downstream release stop | PCA channel 8 | Downstream releases isolated disc to rail |
| Servo positive / negative | Approved servo rail positive / common ground | Verify connector orientation and actual wire colours |
| PCA servo V+ terminal | Approved servo positive | Use board V+ only within validated board/current limits |
| Power negative | Common signal ground | Motor return must not flow through thin sensor ground leads |

Expected OLED address is 0x3C or 0x3D; scan and record it. The actual PCA9685 address depends on address jumpers. Check pull-ups on the shared I2C bus; every pull-up must terminate at 3.3 V. Verify aggregate resistance and clean signals on the actual harness. Avoid boot-strapping, flash and programming pins; this map keeps those free. Confirm that the selected ESP32 board exposes GPIO36 and GPIO39.

GPIO34-39 are input-only and have no internal pull-ups. The user-supplied module schematic shows DO pulled up to VCC; verify each board. Start at 3.3 V. If the module needs 5 V to detect reliably, do not connect its 5 V DO directly to an ESP32 input; agree a level-conversion design first. A 3.3-5 V seller supply claim is not proof of a 3.3 V-safe output when powered at 5 V.

## Power interface owned by teammate

Draw servo power as positive and negative, separate from 3.3 V logic. The teammate must deliver a regulated/otherwise validated voltage compatible with both actual servos and distribution board, adequate current capacity, an accessible cutoff, fuse and appropriate wire/connectors. Battery chemistry, battery count, converter choice and capacity are outside this software plan. Do not route nine MG996R load currents through a breadboard or ESP32 supply pins. If the PCA board cannot carry the measured load, use separate servo power distribution while retaining common ground and PCA signal wires.

Use a current-limited bench supply for unloaded tests. Staggering reduces overlapping motion starts; it does not remove holding/stall current. Measure simultaneous holding load and repeated transitions. Check for ESP32 brownouts and sensor glitches during servo motion. Verify power-off ordering to avoid signal back-powering an unpowered board.

## Before connecting actuators

1. Inspect part identities, board revision, polarity, connectors and labels.
2. Confirm 3.3 V rail and signal-high voltage with a meter.
3. With servo rail disconnected, check OLED and PCA I2C responses.
4. Check every DO at idle and with both red/yellow discs.
5. Disconnect a sensor deliberately: characterize its reading; do not assume software can always identify an unplugged wire.
6. Verify OE pull-up keeps PWM disabled until initialization. OE removes signals, not servo supply; safe retention is a mechanical requirement.
7. Only then calibrate one unloaded servo using future dedicated diagnostic firmware.

## Reflective sensor placement

Vishay specifies peak response near 2.5 mm for the sensor element. The seller's 1-25 mm and 1-8 mm claims are inconsistent and are not acceptance criteria. Begin mounting trials around 2-5 mm from sensor face to the disc's broad face. Verify both colours at the actual gap, speed, tilt and lighting. Aim at solid material away from a centre hole. The chute must constrain wobble without binding the disc.

Mount below each hatch and above the highest final stack. Human and robot discs pass the same sensing zone. An empty chute must not trigger from its far wall. Shield adjacent sensors and external light. A sensor cannot identify colour, final row, motion direction or whether a passing disc later jammed. One disc can generate several raw edges if it wobbles; do not equate every edge with a move.

Tune the comparator until empty remains clear and both colours are detected across the entire mechanical tolerance. If those conditions cannot coexist, change the geometry or sensor technology rather than hiding misses with software. Prefer a true break-beam pair if close reflective mounting cannot be made reliable.

## Sensor and timing worksheet

Fill actual measurements; blanks deliberately mean not calibrated.

| Column | GPIO | Idle / detected V | Gap mm | Red pulse range ms | Yellow pulse range ms | False / miss / double |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 34 | TBD | TBD | TBD | TBD | TBD |
| 2 | 35 | TBD | TBD | TBD | TBD | TBD |
| 3 | 36 | TBD | TBD | TBD | TBD | TBD |
| 4 | 39 | TBD | TBD | TBD | TBD | TBD |
| 5 | 32 | TBD | TBD | TBD | TBD | TBD |
| 6 | 33 | TBD | TBD | TBD | TBD | TBD |
| 7 | 27 | TBD | TBD | TBD | TBD | TBD |

| Timing parameter | Measurement and selection rule | Value |
| --- | --- | --- |
| Detection qualification | Below shortest real-disc pulse, above observed noise; validate resulting miss/false rates | TBD |
| Clear-to-rearm interval | Long enough to suppress one-disc chatter, short enough to observe a second disc separately | TBD |
| Maximum detected duration | Above longest valid passage; held disc beyond it faults | TBD |
| Hatch stagger and travel | Measured motion and supply recovery under load | TBD |
| Hatch clearance margin | Confirm disc clears hatch before closing | TBD |
| Upstream admit interval | Admit exactly one disc across low/full stack and friction conditions | TBD |
| Upstream closing interval | Stop fully retains stack before downstream release | TBD |
| Downstream release/close | Isolated disc clears stop before it returns | TBD |
| Robot delivery timeout | Above maximum measured valid target passage time plus documented margin | TBD |
| Board settling | Measured worst valid settling plus margin | TBD |
| Button debounce | Filter observed bounce without rejecting deliberate presses | TBD |

No sensor-only filter can reliably separate touching discs with no clear gap. That case depends on magazine isolation and human-use constraints; pause when evidence is ambiguous. Choose a calibration baseline from at least 100 real drops of each colour on one column, then repeat on every column. Keep raw timestamps, configuration, lighting and geometry notes.

## Servo calibration worksheet

Start unloaded with conservative travel, move in small pulse-width increments, and avoid mechanical hard stops. Record endpoints in microseconds rather than assuming nominal angles. A starting frequency near 50 Hz is a test candidate, not a calibrated value. Verify actual servo requirements, PCA oscillator timing and pulse measurements.

| Channel | Role | Hold/closed us | Open/release us | Travel/settle ms | Loaded current / notes |
| --- | --- | --- | --- | --- | --- |
| 0 | Column 1 hatch | TBD | TBD | TBD | TBD |
| 1 | Column 2 hatch | TBD | TBD | TBD | TBD |
| 2 | Column 3 hatch | TBD | TBD | TBD | TBD |
| 3 | Column 4 hatch | TBD | TBD | TBD | TBD |
| 4 | Column 5 hatch | TBD | TBD | TBD | TBD |
| 5 | Column 6 hatch | TBD | TBD | TBD | TBD |
| 6 | Column 7 hatch | TBD | TBD | TBD | TBD |
| 7 | Upstream stop | TBD | TBD | TBD | TBD |
| 8 | Downstream stop | TBD | TBD | TBD | TBD |

Check no sustained buzzing, excessive heating, hard-stop load or bounce. Confirm physically that removing PWM or power does not unexpectedly release the magazine. A PCA command provides no position feedback; mechanical retention cannot be replaced by an OE setting.

## Records to carry into the hackathon

Keep a signed column/channel label sheet, photos of connectors, calibration table, tested library/core versions, raw serial logs and hardware revision. Use one configuration source for calibrated values when firmware is eventually written. Requalify after sensor adjustment, mechanical changes, power changes or servo replacement.
