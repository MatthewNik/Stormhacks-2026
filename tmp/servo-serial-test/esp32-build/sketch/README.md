#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ServoSerialTest\\README.md"
# Serial servo bench test

Standalone ESP32 DevKit V1 test for eight servos on PCA9685 connectors 0-7. The sensor, game controller and Raspberry Pi are not used by this sketch.

## Wiring

Disconnect USB and bench-supply output before wiring. Use the printed labels, not header positions.

| From | To |
| --- | --- |
| ESP32 3V3 | PCA VCC (logic) |
| ESP32 GND | PCA GND |
| ESP32 D21 | PCA SDA |
| ESP32 D22 | PCA SCL |
| ESP32 D25 | PCA OE |
| 10 kOhm resistor | Between PCA OE and ESP32 3V3 |
| Bench supply positive | PCA V+ screw terminal |
| Bench supply negative | PCA GND screw terminal |
| Servo orange | Selected channel PWM / S |
| Servo red | Selected channel V+ / + |
| Servo brown | Selected channel GND / - |

Verify the OE pull-up holds OE HIGH during reset, including the effect of any onboard pull-down. OE HIGH disables control signals; it does not remove motor power or guarantee holding torque. Follow the board's output labels for plug orientation.

Power the ESP32 from computer USB. Start servo power at **4.8 V**, verified before connection. The supplied Miuz ei package specifies 4.8-6.0 V: never exceed 6.0 V. The adjustable supply's 32 V maximum must not be applied to this circuit. Keep servo V+ separate from logic VCC/3V3. All grounds must be connected.

Use the breadboard for logic connections only. Run servo power directly to suitable distribution wiring. The package recommends at least 1 A supply capacity per servo (at least 8 A capacity for eight); supply capacity does not mean that current is forced through each servo. The earlier 0.5 A current limit is only a cautious initial unloaded single-servo test setting, not an eight-servo setting. If current limiting occurs, turn output off and inspect before raising the limit. Verify the actual PCA board, screw terminals, connectors and wiring can carry the measured total current; use separate servo power distribution when needed. A 10 A supply rating does not establish board capacity.

## Arduino IDE

1. Install **esp32 by Espressif Systems** using Boards Manager.
2. Select **ESP32 Dev Module** and the board's current USB port.
3. In Library Manager install **Adafruit PWM Servo Driver Library** and **Adafruit BusIO**; accept dependencies.
4. Open `ServoSerialTest.ino` in this folder. `ServoConfig.h` stays beside it.
5. With servo supply output off, verify and upload the sketch.
6. Open Serial Monitor at **115200 baud**, with **Newline**, **Carriage Return**, or **Both NL & CR**. Wait for `READY`.

No startup sweep or centering occurs. Opening Serial Monitor may reset the ESP32; startup always disables outputs. PCA address defaults to 0x40; adjust ServoConfig.h if address jumpers change it.

## Commands

```text
servo 0 90
servo 1 50
servo 7 90
off 1
off all
help
```

Channel numbers match PCA labels **0-7**, not one-based servo numbering. Commands are lowercase and angles must be integers from 0 through 180. `servo 1 50` commands PCA connector 1 to approximately 50 degrees. A commanded servo retains its pulses until its next command or `off`; other channels are unchanged. Channels 8-15 stay disabled.

The package specifies 50 Hz and 500-2500 microseconds for nominal 180-degree travel. Defaults map 0 -> 500 us, 90 -> 1500 us and 180 -> 2500 us. Physical angle is approximate: servo variation and PCA oscillator accuracy require calibration. Adjust each channel's endpoints independently in ServoConfig.h, staying within the package range. Narrow the range if needed to avoid hard stops. There is no position feedback, so `OK` reports a successful command, not confirmed shaft movement.

Malformed commands and lines exceeding 95 characters are rejected without changing outputs. `off all` disables OE and clears all 16 channels. `off <channel>` stops that channel's pulses. Neither command disconnects servo power.

Detected initialization/write errors or failure of the once-per-second PCA presence check latch a fault and disable OE. Repair with power off and reset the ESP32 to reinitialize; commands cannot clear the fault. This detects communication failures, not servo jams, missing servo power, actual position, or every possible PCA register fault. The external supply cutoff remains necessary.

## Bench verification

1. With servo supply off, confirm `READY`; check invalid commands below do not activate any output.
2. Connect one unloaded servo to channel 0; ensure the horn has free clearance. Enable 4.8 V servo power, then send `servo 0 90`.
3. Send `servo 0 50`; test small angle increments toward both endpoints. Stop if the servo buzzes, binds or heats. Avoid driving an attached mechanism into its stops.
4. Test `off 0`, then another valid position, then `off all`. Verify resetting produces no automatic movement.
5. Test each connector individually, including channel 7; then verify commanding one channel leaves another channel holding its existing command.
6. Check Newline, CR and CRLF; repeated commands; a line longer than 95 characters followed by a valid command; blank lines and whitespace.
7. Reject `servo -1 90`, `servo 8 90`, `servo 0 -1`, `servo 0 181`, `servo x 90`, `servo 0 50.5`, `servo 0 90 extra`, `off 8` and `help extra`.
8. With power disconnected, unplug PCA logic; reconnect USB and confirm initialization faults with OE disabled. Repair wiring with power off and reset. Never induce a wiring short as a fault test.
9. Before eight-servo testing, verify power distribution and measure current/rail stability. Begin with individual commands and no mechanical load.

Useful references: [Adafruit PCA pinout](https://learn.adafruit.com/16-channel-pwm-servo-driver/pinouts), [driver API](https://adafruit.github.io/Adafruit-PWM-Servo-Driver-Library/html/class_adafruit___p_w_m_servo_driver.html).
