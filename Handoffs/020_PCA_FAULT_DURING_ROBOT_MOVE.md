# Handoff 020: PCA fault whenever the robot starts its move

## Symptom

On build 018, the controller latched `ERROR: PCA fault` as soon as the robot began a move, whether the robot went first or answered a human disc. Putting the disc back into the indexer made it fault right away. MotorCommandTest drives every motor on the same PCA without faulting.

## What the robot move does that nothing else does

Human turns leave all seven doors open. RobotOpening then closes six doors and opens the target, starting the commands only 50 ms apart. With servo travel of roughly 150–300 ms, all six servos were moving and pulling stall current together. Startup and human turns either do not move the doors or happen before servo power is switched on.

## Comparing it with MotorCommandTest

| Difference | Matters? | Why |
| --- | --- | --- |
| One servo per typed command vs six closing within 300 ms | Yes, the trigger | A six-servo current surge disturbs the servo supply and the shared ground and puts noise on the I2C lines. That is the only event that lines up with "every robot move". |
| 7 IR inputs on CHANGE GPIO interrupts vs none | Yes, the amplifier | The ISR ran on every edge, even while capture was off. An LM393 comparator sitting near its threshold (disc partly in a beam, ground bounce from servo current) can toggle at tens to hundreds of kHz. The ISR, the I2C driver interrupt and loopTask all share core 1, so a storm can delay I2C completion past the 50 ms Wire timeout. The test program has no such load. The disc-in-indexer trigger fits a beam sitting at threshold. |
| Health check also reads MODE1 and has a STOP-separated fallback | No | The sleep bit is only set after a real PCA reset, and the motor program would catch that same reset through PRE_SCALE. The fallback only adds tolerance. Neither causes a fault on a healthy bus. |
| No OLED/SPI or sensor activity | Minor | SPI uses separate pins and no interrupt-heavy driver. It only lengthens loop time. |

The real weakness was zero tolerance. A single corrupted or timed-out I2C transaction during a current spike latched a permanent fault. The motor program has the same zero tolerance but never creates the spike.

## Changes (build 019)

- `Esp32Sensors.h`: no more GPIO edge interrupts. A 250 us hardware timer samples all seven pins with the same 2 ms debounce and queue logic. The cost is a fixed 7 pin reads per sample however noisy a line is. If the timer cannot start, the controller faults at boot.
- `GameConfig.h`: `COMMAND_GAP_MS` goes from 50 to 150, so about two servos travel at once. A full door batch now takes about 1.35 s.
- `PcaHardware.h`: each register read and PWM write gets up to 3 attempts, 0.5 ms apart. A changed prescaler or sleep bit is re-read before it is believed. Only a failure that lasts through every attempt latches the fault. The detail line now includes `retries=` and names a confirmed power-on prescaler (0x1E) as `PCA reset detected`.
- Sketch: the 1 s bus check prints `WARNING: PCA I2C retried N time(s)` whenever the retry count rises, so a marginal supply shows up before it becomes a fault.
- Tests: new cases for transient and persistent read/write failures, one corrupted byte, reset detection, timer-driven sampling, and fixed per-sample cost. All 5 native suites and 24 Pi tests pass. ESP32 build succeeded (344 KB). Artifacts were rebuilt in `working code/deployment/`.

## What to check on hardware

1. Flash build 019 as in `working code/DEPLOY.md` and confirm the boot line.
2. Play a robot move with the disc loaded.
   - No fault and no warning means the change fixed it.
   - `WARNING: PCA I2C retried` lines mean the bus is still being disturbed but recovering. Improve servo supply current, use short and thick ground returns from the servo supply to the PCA GND terminal, and add a large capacitor (470–1000 uF) across PCA V+/GND.
   - A fault with `PCA reset detected` or `sleep mode` means the PCA logic supply itself dropped. Check the ESP32 3V3 rail that feeds PCA VCC and the IR modules, and make sure servo current does not return through the logic ground.
   - A fault with `read failed ... retries=` means the bus stayed broken for all 3 attempts. That is the same wiring and supply problem, only more severe.
3. Check the bench supply current limit. At about 1 A per servo, several moving at once can push the supply into current limiting.
