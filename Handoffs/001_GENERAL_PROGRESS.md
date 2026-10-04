# Handoff 001: general progress

Recorded October 3, 2026.

## Current state

**The SPI color OLED is working.** The ESP32 currently has [ColorOledSpiTest](../Arduino/ColorOledSpiTest/ColorOledSpiTest.ino) uploaded on **COM8**. The upload's flash hashes passed verification, serial messages showed the test drawing pages, and the operator confirmed visible display operation.

The active project is `C:\Users\matth\Documents\Stormhacks2026 Oct 3-4`. Read [AGENTS.md](../AGENTS.md) before continuing. Keep new files and outputs here, keep Arduino sketches organized, and use clearly named functions and variables.

ESP32 remains authoritative for the future game and hardware controller. Initial testing is directly from the computer. Raspberry Pi support for Gemini and ElevenLabs is a later optional addition; network failures must not disable core gameplay or bypass ESP32 move/safety logic. The current firmware consists of separate bench tests, not an integrated Connect Four controller.

See [successful wiring](002_SUCCESSFUL_WIRING.md) for the confirmed display connections and the status of other wiring.

## Test sketches and results

| Test | Behavior | Verification and current status |
| --- | --- | --- |
| [ServoSerialTest](../Arduino/ServoSerialTest/ServoSerialTest.ino) | `servo <0-7> <0-180>`, `off <channel>`, `off all`, `help`; 115200 baud | ESP32 compilation passed. Native tests of the actual parser and simulated hardware passed. The operator reported motors not moving; physical servo operation has not been confirmed in this record. |
| [Tcrt5000SerialTest](../Arduino/Tcrt5000SerialTest/Tcrt5000SerialTest.ino) | Raw HIGH-N/LOW-N readings for seven inputs, refreshed every 50 ms | Updated version compiled successfully. No confirmed sensor bench result or upload is recorded here. |
| [OledDisplayTest](../Arduino/OledDisplayTest/OledDisplayTest.ino) | SH1106, 128x64, I2C scan and drawing test | Compilation passed. This test was observed running on COM8, but the actual display remained blank. It targets a different display than the one supplied. |
| [ColorOledSpiTest](../Arduino/ColorOledSpiTest/ColorOledSpiTest.ino) | SSD1331, 96x64, SPI; red, green, blue, text/border and color bars every 2 seconds | Compilation and upload passed. Operator confirmed the display works. This is the currently uploaded sketch. |

Uploading a test replaces the firmware running on the ESP32. Keeping multiple sketch folders does not make their features run together.

### Servo test details

- PCA connector numbers are zero-based: `servo 1 50` means connector **1**, not connector 0.
- Defaults follow the supplied Miuz ei servo package: **50 Hz**, **500-2500 us**, **1500 us neutral**, nominal **180-degree travel**, and **4.8-6.0 V** operation. Individual endpoints are configurable in ServoConfig.h; physical angles still require calibration.
- Startup leaves all channels off. Only a valid servo command enables OE; channels 8-15 stay off. Initialization/write failures or a failed periodic PCA presence check disable outputs and require reset.
- Native assertions passed for channel selection, pulse mapping, invalid arguments, extra tokens, CR/LF/CRLF, oversized/control-character rejection, recovery after invalid input, startup, off commands and latched communication faults.
- These checks do not establish servo power, connector orientation, actual shaft movement or mechanical suitability.

### Sensor test details

- Sensor numbering maps to GPIO **34, 35, 36, 39, 32, 33, 27** for sensors **1-7**.
- The original requested object-presence inversion and change-only reporting were replaced with raw pin levels and a **50 ms** refresh, as requested.
- Expected active-low modules produce **LOW-N when detecting** and **HIGH-N when clear**. Actual polarity must be checked physically.
- All seven entries are enabled by default. Disable entries for disconnected sensors in SensorConfig.h to avoid floating readings. There is no debounce or passage counting; pulses between samples can be missed.

## Attempts, failures and corrections

### Display mismatch resolved

The initial hardware description and planning documents specified a four-pin SH1106 I2C OLED. An I2C test was created for that assumption. Its startup scan reported **0x3C** and **0x40**, and the SH1106 initialization path completed, but the operator reported a blank screen even after reset.

Photos then established that the actual module is marked **0.95 inch 96x64 OLED V2.0**, with **GND, VCC, SCL, SDA, RES, DC, CS**. This required a separate SPI test with an SSD1331 driver. The operator rewired the display, the SPI test was uploaded, and visible output was confirmed.

The working test establishes SSD1331-compatible operation; the exact chip marking was not read. The earlier 0x3C response was not traced to a particular physical device and must not be treated as this SPI display's address. An I2C acknowledgement alone did not establish a working screen.

### Servo issue remains unverified

The operator reported that servo commands did not move the motors. Wiring guidance and diagrams emphasized separate PCA VCC/V+ supplies, common ground, correct plug orientation and OE on D25. No later confirmation of motor movement was provided. Do not mark this issue resolved because the PCA responded on I2C or because software tests passed.

### Wiring diagrams

The saved diagrams are in [ServoSerialTest/images](../Arduino/ServoSerialTest/images). Drafts with ambiguous or incorrect wire endpoints were rejected and revised before the selected images were saved. Use the pin labels and written tables; the diagrams are logical connections, not exact breadboard-hole layouts. They cover sensors and servos, not the actual SPI display.

### Planning records

The active workspace initially contained AGENTS.md and four planning documents. Their references to existing firmware, bundled Minimax, PDFs, diagrams and previous verification were treated as historical references, not proof that those artifacts or tests existed in the active workspace. The authorized preparation PDF was consulted as reference; no old project implementation was copied into the active workspace.

The planning documents still describe a **SH1106 I2C display** and **nine servo roles**. Actual display integration must use the working **96x64 SPI wiring**. The present servo bench test exposes only **eight channels**; the planned seven hatches plus two magazine stops require a ninth output before full integration.

## Environment and organization

- Board observed during upload: **ESP32-D0WD-V3**, DevKit V1 headers; Arduino target **ESP32 Dev Module** (`esp32:esp32:esp32`). Current USB port: **COM8**; confirm after reconnection.
- All test sketches use **115200 baud**. The sensor and display tests hold **D25 HIGH**, disabling a PCA connected through OE.
- Verified build dependencies: ESP32 core **3.3.12**, Adafruit PWM Servo Driver **3.0.3**, BusIO **1.17.4**, GFX **1.12.6**, SH110X **2.1.15**, SSD1331 **1.3.0**, as relevant to each sketch.
- The official SSD1331 dependency and license are retained under `Arduino/libraries/Adafruit-SSD1331-OLED-Driver-Library-for-Arduino-1.3.0`. Arduino IDE may still require Library Manager installation to find it; command-line compilation used this workspace copy.
- Build outputs, downloaded dependency archive and temporary compiler configurations are under `tmp/servo-serial-test`, `tmp/tcrt5000-serial-test`, `tmp/oled-display-test` and `tmp/color-oled-spi-test`.
- Configuration headers sit beside their sketches; per-test README files contain usage and wiring instructions.

## Next work

1. Validate each sensor's power, polarity and number using the sensor sketch; account for disconnected inputs.
2. Resolve the motor issue with one unloaded servo and verify supply voltage, OE and plug orientation before expanding to eight.
3. Record measured servo endpoints and sensor behavior; do not reuse nominal values as mechanical calibration.
4. Integrate the confirmed SPI display and tested sensors/actuators into a new controller while preserving the ESP32 architecture and recovery rules.
5. Before game integration, reconcile the planning documents with the actual display and required nine-servo arrangement.
