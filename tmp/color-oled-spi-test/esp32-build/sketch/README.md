#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\README.md"
# 96x64 SPI color OLED test

For the pictured module labelled **0.95 inch 96x64 OLED V2.0** with **GND, VCC, SCL, SDA, RES, DC, CS**. Its size and connector are consistent with an SSD1331 SPI color display; the controller identity remains an inference until confirmed by its seller documentation or successful operation. This is not the planned four-pin SH1106 128x64 I2C OLED.

## Wiring

Disconnect USB and servo power before rewiring. Remove the display's SCL/SDA from the PCA's D22/D21 breadboard connections. Follow the display's printed pin names:

| OLED pin | ESP32 connection | Function |
| --- | --- | --- |
| GND | Common ground rail / GND | Ground |
| VCC | 3.3 V rail / 3V3 | Module power |
| SCL | D18 / GPIO18 | SPI clock, not I2C SCL |
| SDA | D19 / GPIO19 | SPI MOSI data, not I2C SDA |
| RES | RX2 / GPIO16 | Reset |
| DC | TX2 / GPIO17 | Data/command selection |
| CS | D26 / GPIO26 | Chip select |

RX2 and TX2 are used as ordinary GPIOs here, not UART signals. The custom SPI data pin is deliberately D19, leaving the planned Start button on D23 free. Do not substitute default SPI pin mappings without editing DisplayConfig.h. No MISO connection is needed.

The OLED can share the **3.3 V and ground rails** with PCA logic and sensors. It cannot share the PCA's **I2C signal wires**. Keep PCA SDA on D21, SCL on D22 and OE on D25 with its existing pull-up. Each signal can use its own breadboard row; separate signals must not share a connected five-hole row. Keep all servo power disconnected for this test. Never connect OLED VCC to servo V+.

## Setup and operation

1. Install **Adafruit SSD1331 OLED Driver Library for Arduino**, **Adafruit GFX Library** and **Adafruit BusIO** using Library Manager, accepting dependencies.
2. Open `ColorOledSpiTest.ino`, select **ESP32 Dev Module**, and select the current USB port.
3. Recheck all seven connections against the table, then upload.
4. Open Serial Monitor at **115200 baud**. The screen should cycle red, green, blue, text/border and RGB bars every two seconds.

GPIO25 stays HIGH to disable PCA outputs. There are no servo commands or sensor readings. Uploading replaces the firmware currently on the ESP32; the other sketch folders remain available.

This is write-only SPI: serial messages mean initialization/drawing commands were sent, not that the module acknowledged them. There is no display I2C address to scan. The earlier 0x3C scan response cannot identify this SPI module; disconnecting devices one at a time with power off is needed to establish which I2C device produced it.

## If it remains blank

- Measure about 3.3 V between VCC and GND at the actual display pins.
- Verify RES, DC and CS are all wired; leaving them floating can prevent operation.
- Verify SCL/SDA were moved to D18/D19, not left on D22/D21.
- Check continuity and breadboard row/rail splits with power off.
- Confirm controller identity from the seller if this SSD1331 test still gives no visible output. A photo establishes the printed resolution and connector, not a guaranteed controller type.

Reference: [Adafruit 96x64 SSD1331 color OLED guide](https://learn.adafruit.com/096-mini-color-oled), [SSD1331 library](https://github.com/adafruit/Adafruit-SSD1331-OLED-Driver-Library-for-Arduino).
