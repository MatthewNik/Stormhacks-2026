# Successful wiring record

Recorded October 3, 2026. Update this record when a hardware test is actually confirmed.

## Confirmed working: SPI color OLED

**Result:** the operator confirmed visible display operation after the SPI test was uploaded to COM8. The module is labelled **0.95 inch 96x64 OLED V2.0**. The working driver is **Adafruit SSD1331**; the display is **96x64**, not the SH1106 128x64 module described in the original plan.

Firmware: [ColorOledSpiTest](../Arduino/ColorOledSpiTest/ColorOledSpiTest.ino). Configuration: [DisplayConfig.h](../Arduino/ColorOledSpiTest/DisplayConfig.h). Full setup: [README](../Arduino/ColorOledSpiTest/README.md).

| Display pin | Successful ESP32 connection | Meaning |
| --- | --- | --- |
| GND | Common ground / GND | Ground |
| VCC | 3V3 / 3.3 V logic rail | Display power |
| SCL | D18 / GPIO18 | SPI clock |
| SDA | D19 / GPIO19 | SPI MOSI data |
| RES | RX2 / GPIO16 | Reset |
| DC | TX2 / GPIO17 | Data/command |
| CS | D26 / GPIO26 | Chip select |

The SCL/SDA labels on this display refer to its SPI signals. **Do not connect them to the PCA9685's D22/D21 I2C lines.** RX2 and TX2 are ordinary GPIO outputs in this test. No MISO wire is needed. D19 is explicitly configured as MOSI; do not replace it with D23 unless the firmware and future button allocation are changed together.

Expected test: full red, green and blue screens, a text/border page, and RGB bars, changing every two seconds. Serial Monitor runs at 115200 baud. Upload flash verification and drawing messages were observed; the separate visual confirmation established that the screen actually worked.

## Confirmed communication: PCA9685

The shared-bus scan found the PCA at **0x40**. This confirms an I2C response, not successful servo motion or measured power behavior.

| PCA connection | Assigned connection | Verification status |
| --- | --- | --- |
| VCC | ESP32 3V3 / breadboard 3.3 V rail | Used in the intended I2C logic setup; voltage not independently measured here |
| GND | Common ESP32 ground | Intended common reference; continuity not independently measured here |
| SDA | D21 / GPIO21 | PCA response observed on this bus |
| SCL | D22 / GPIO22 | PCA response observed on this bus |
| OE | D25 / GPIO25, with 10 kOhm pull-up to 3.3 V | Required by the test sketches; physical HIGH/LOW levels not measured here |

The source of the scan's separate **0x3C** response was not isolated. Do not attribute it to the now-working SPI display.

## Servo wiring: retained for validation, not confirmed working

The operator reported motors not moving. There is no recorded follow-up confirming servo movement. Use [ServoSerialTest](../Arduino/ServoSerialTest/ServoSerialTest.ino) to finish validation.

| Connection | Intended wiring |
| --- | --- |
| Bench supply positive | PCA V+ screw terminal |
| Bench supply negative | PCA GND screw terminal, with common ESP32 ground |
| Servo orange | Selected PCA channel PWM / S |
| Servo red | Selected PCA channel V+ / + |
| Servo brown | Selected PCA channel GND / - |

Start at **4.8 V**. The supplied Miuz ei package specifies **4.8-6.0 V**, **50 Hz**, and **500-2500 us**. The supply's **32 V / 10 A maximum capability** is not an operating voltage setting. Never apply 32 V to this setup. The package recommends at least 1 A supply capacity per servo; verify actual board, connector and distribution capacity before using eight servos.

**PCA VCC is 3.3 V logic; V+ is separate servo power. Never join the positive rails.** Servo power/current must not run through the breadboard or ESP32 supply pins. Ground remains common, with suitable direct motor-return wiring.

The test supports PCA connectors **0-7**, with no game-specific mechanism roles assigned by this bench test. Confirm the selected connector matches the command. OE should go LOW after a valid servo command; if it remains HIGH, pulses are disabled. `off` disables pulses, not servo supply, and does not guarantee mechanical holding.

## Sensor wiring: map retained for validation

No confirmed hand-wave or disc-passage result is recorded yet. Firmware: [Tcrt5000SerialTest](../Arduino/Tcrt5000SerialTest/Tcrt5000SerialTest.ino).

All sensor VCC pins connect to the breadboard **3.3 V** rail, all GND pins connect to common ground, and AO remains unused. Each DO connects independently:

| Sensor number | ESP32 DO input | Serial suffix |
| --- | --- | --- |
| 1 | D34 / GPIO34 | -1 |
| 2 | D35 / GPIO35 | -2 |
| 3 | VP / GPIO36 | -3 |
| 4 | VN / GPIO39 | -4 |
| 5 | D32 / GPIO32 | -5 |
| 6 | D33 / GPIO33 | -6 |
| 7 | D27 / GPIO27 | -7 |

The sketch reports raw levels every **50 ms**: expected active-low modules read **LOW-N with an object** and **HIGH-N when clear**. Verify each module's actual polarity and 3.3 V operation. GPIO34-39 have no internal pull-ups; verify the module's output pull-up. Disable disconnected entries in SensorConfig.h to avoid floating-input messages. Do not connect a 5 V DO output directly to the ESP32.

## Breadboard and current pin allocation

- Share the 3.3 V and ground rails for logic/display/sensor connections, within the actual supply/regulator capacity.
- For the PCA, use one connected row for D21/SDA and a different connected row for D22/SCL. OLED SPI signals each use separate connections from those rows.
- Five-hole rows across the breadboard center gap are separate. Rails may also split; bridge matching sections only, never positive to ground.
- Disconnect power before changing wiring. Follow printed module labels rather than assuming physical connector order.

| Function | GPIO allocation | Status |
| --- | --- | --- |
| Working SPI OLED | 18, 19, 16, 17, 26 | Visible operation confirmed |
| PCA I2C | 21, 22 | Address response confirmed |
| PCA OE | 25 | Configured in all current test sketches |
| Seven sensors | 34, 35, 36, 39, 32, 33, 27 | Awaiting physical confirmation |
| Planned buttons | 13, 14, 23 | Reserved by plan; not tested in this record |

These allocations avoid overlap, but simultaneous integrated operation still needs testing. The original full mechanism calls for nine servos; the current eight-channel bench test does not yet cover PCA channel 8 for the second magazine stop.
