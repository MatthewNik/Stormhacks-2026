#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\OledDisplayTest\\README.md"
# SH1106 OLED display test

Standalone test for the project's planned **1.3-inch SH1106, 128 x 64, four-pin I2C OLED**. Pin labels alone do not establish the controller type. If your actual module is SSD1306 or another controller, this driver needs changing.

## Shared breadboard wiring

Yes, the OLED and PCA9685 can share SDA and SCL on the same ESP32. Keep each signal separate:

| Breadboard connection | Connect together |
| --- | --- |
| 3.3 V rail | ESP32 3V3, OLED VCC, PCA VCC |
| Ground rail | ESP32 GND, OLED GND, PCA GND |
| One separate five-hole row for SDA | ESP32 D21, OLED SDA, PCA SDA |
| Another separate five-hole row for SCL | ESP32 D22, OLED SCL, PCA SCL |

Use connected holes within the same side of the breadboard's center gap. SDA and SCL must use DIFFERENT rows, not the same five-hole strip, and neither belongs on a power rail. If you run out of holes, bridge another row for that same signal. The two sides across the center gap are not automatically connected. Keep wires short and follow the display's printed pin labels.

Use **3.3 V** for both OLED VCC and PCA logic VCC so onboard bus pull-ups terminate at 3.3 V. Do not attach the OLED to servo V+. Check actual module voltage compatibility and total ESP32 regulator load when other sensors are connected. PCA OE remains connected to D25 with the 10 kOhm pull-up to 3.3 V. Keep servo power disconnected during this display-only test.

Devices share wires because they use different addresses: OLED normally 0x3C or 0x3D, PCA normally 0x40. A PCA can also acknowledge its group/all-call address 0x70; that does not necessarily mean another device is present. The sketch scans the bus, then selects the only responding OLED candidate among 0x3C and 0x3D. If both respond, set `OLED_ADDRESS` explicitly in DisplayConfig.h. A responding address does not prove the device is an SH1106.

## Upload and expected result

1. Open `OledDisplayTest.ino` in Arduino IDE. Keep DisplayConfig.h beside it.
2. Select **ESP32 Dev Module** and the current board port.
3. Install **Adafruit SH110X**, **Adafruit GFX Library** and **Adafruit BusIO** through Library Manager. Accept dependencies.
4. Verify and upload with servo power disconnected.
5. Open Serial Monitor at **115200 baud**. It prints found I2C addresses and READY or a wiring/initialization error. No commands are required.

The OLED cycles three pages every three seconds: text/address with an updating uptime and progress bar, outlined/filled shapes with an edge border, and an 8-pixel checkerboard. Frames refresh every 250 ms at 100 kHz bus speed. Check text legibility, all four edges, and the patterns across the full display.

GPIO25 stays HIGH to disable a connected PCA via OE. The sketch does not initialize the PCA or send servo commands. Uploading replaces the previous test running on the ESP32; other test folders are unchanged.

## Troubleshooting and bench checks

- No addresses: check 3.3 V, common ground, SDA/SCL routing and power-rail splits.
- Only PCA appears: check the OLED connections and module power requirements.
- OLED responds but remains blank/garbled: verify controller and resolution, power, and wiring; a successful scan alone is not a display test.
- With both boards attached, confirm OLED and PCA addresses appear and the OLED cycles correctly; no servo should move.
- A lost OLED response stops drawing and reports an error. Repair with power off, then reset. The display library does not report every frame-transfer error, so visually check operation.

References: [ESP32 I2C](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html), [PCA pinout](https://learn.adafruit.com/16-channel-pwm-servo-driver/pinouts).
