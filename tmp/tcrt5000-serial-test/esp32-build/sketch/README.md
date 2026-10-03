#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\Tcrt5000SerialTest\\README.md"
# TCRT5000 digital sensor test

Open `Tcrt5000SerialTest.ino` in Arduino IDE. Select **ESP32 Dev Module**, choose the board's current USB port, and upload. Open Serial Monitor at **115200 baud**. No extra libraries or serial commands are needed.

The sketch prints the initial state of every enabled sensor, then refreshes all enabled readings every **50 ms** (20 batches per second), even when the readings do not change:

```text
LOW-1
HIGH-1
LOW-1
```

Messages are raw GPIO levels. For the expected active-low modules, `LOW-1` means sensor 1 detects an object (such as your hand); `HIGH-1` means it is clear. Each sensor is independent. Verify your actual module polarity: a different module may behave differently.

## Wiring and sensor numbers

| Sensor | Module DO to ESP32 | Output |
| --- | --- | --- |
| 1 | D34 / GPIO34 | HIGH-1 / LOW-1 |
| 2 | D35 / GPIO35 | HIGH-2 / LOW-2 |
| 3 | VP / GPIO36 | HIGH-3 / LOW-3 |
| 4 | VN / GPIO39 | HIGH-4 / LOW-4 |
| 5 | D32 / GPIO32 | HIGH-5 / LOW-5 |
| 6 | D33 / GPIO33 | HIGH-6 / LOW-6 |
| 7 | D27 / GPIO27 | HIGH-7 / LOW-7 |

All module VCC pins connect to the breadboard **3.3 V** rail, all GND pins to common ground, and ESP32 3V3/GND feed those rails. Leave AO unused. Keep each DO separate. Use the module's actual printed labels rather than assuming a connector order. Start at 3.3 V and verify detection/output voltage; do not put a 5 V DO signal on an ESP32 input.

All seven inputs are enabled by default. If only your D34 sensor is connected, set `enabled` to `false` for sensors 2-7 in `SensorConfig.h`, then upload again. Unconnected inputs can float and produce meaningless messages. GPIO34-39 have no internal pull-ups; verify the module has a working output pull-up to 3.3 V. This test cannot distinguish an unplugged sensor from an unobstructed sensor reliably.

## Adjusting detection

The sketch does not invert, filter, or reinterpret digital levels. Confirm the module DO goes **LOW when detecting** before using LOW as a detection message.

Tune the module's sensitivity screw while moving your hand or a disc near the sensing face. A reflective TCRT5000 is intended for close-range detection; waving at a distance may not trigger it. Try a few millimetres first and check under actual lighting. Change `REPORT_INTERVAL_MS` in SensorConfig.h to adjust the refresh period. This samples the inputs once per reporting interval and can miss pulses between samples; it is a hand-wave test, not calibrated disc-passage detection.

Keep servo power disconnected during sensor tests. The sketch holds GPIO25 HIGH so a PCA9685 wired with OE to D25 stays disabled. It does not send servo commands or initialize I2C. Uploading replaces the servo-test firmware on the ESP32; the separate servo sketch files remain available to upload later.

## Bench checks

- With a clear active-low sensor, verify HIGH-N; bring an object near it and verify LOW-N; remove it and verify HIGH-N.
- Check all seven mappings individually, then two sensors at once.
- A steady state should repeat every 50 ms; with seven enabled sensors this produces 140 lines per second.
- If readings chatter, tune placement/sensitivity and check power/lighting. No software debounce is applied.

Physical detection, voltage and polarity still require hardware verification.
