# Full hardware wiring

Use this guide with `working code`, the full hardware firmware. The sensor map comes from the planning wiring contract and handoff 002; the SPI OLED and single magazine motor reflect the newer handoffs and current calibration. Disconnect USB and servo power before changing wiring.

## Columns viewed from the front

Columns are numbered right to left. Sensor DO and motor output must refer to the same physical column.

| Position from front | Column | PCA hatch | IR DO GPIO | Closed/down | Open/up |
| --- | --- | --- | --- | --- | --- |
| Rightmost | 1 | 0 | 34 (D34) | 50° | 140° |
| Second from right | 2 | 1 | 35 (D35) | 60° | 145° |
| Third from right | 3 | 2 | 36 (VP) | 55° | 140° |
| Centre | 4 | 3 | 39 (VN) | 55° | 147° |
| Third from left | 5 | 4 | 32 (D32) | 55° | 140° |
| Second from left | 6 | 5 | 33 (D33) | 55° | 141° |
| Leftmost | 7 | 6 | 27 (D27) | 70° | 140° |

PCA7 is the single magazine indexer: load 80°, unload/release 145°. PCA8–15 remain disabled. All seven hatch outputs are enabled. Servo PWM mapping is 50 Hz, 500–2500 us. Flap command starts are staggered by at least 50 ms; motor 7 retains its separate delivery sequence.

## PCA9685 and motor power

| PCA connection | ESP32 / supply |
| --- | --- |
| VCC (logic) | ESP32 3V3 |
| GND | Common ESP32 / external supply ground |
| SDA | GPIO21 |
| SCL | GPIO22 |
| OE | GPIO25, plus external 10 kOhm pull-up to 3.3 V |
| V+ (servo power) | Regulated external servo supply, initially 4.8 V, within actual servo ratings |

Expected I2C address is 0x40. Connect each servo orange/yellow wire to its channel S/PWM, red to servo V+, and brown/black to GND. Never join V+ to ESP32 3V3, 5V or USB positive. Servo current must use suitable direct supply/return wiring, not ESP32 power pins or breadboard logic rails. OE disables pulses, not electrical servo power or guaranteed mechanical holding.

## IR modules

All seven sensor VCC pins connect to 3.3 V, GND to common ground, AO remains unused, and DO follows the column table. Inputs are configured active LOW. Verify clear = HIGH and disc detected = LOW on every module. GPIO34–39 have no internal pull-ups; verify module pull-ups to 3.3 V or provide suitable external ones. Never connect a 5 V DO signal directly to ESP32.

Robot delivery requires exactly one qualified passage at the target column; wrong/extra/missing/stuck events pause delivery. Human-turn IR capture remains disabled as specified by handoff 004; enter the human column through the Pi terminal after placement. Clear sensor values alone do not prove connectivity or final seating.

## OLED and three buttons

| OLED pin | ESP32 |
| --- | --- |
| VCC / GND | 3.3 V / common ground |
| SCL (SPI clock) | GPIO18 |
| SDA (SPI data) | GPIO19 |
| RES | GPIO16 (RX2) |
| DC | GPIO17 (TX2) |
| CS | GPIO26 |

This is the working SSD1331 96x64 SPI display, not the old SH1106 I2C plan. The display's SCL/SDA must not use PCA's GPIO22/21.

Left GPIO13, Right GPIO14, Centre GPIO23: each switch connects its GPIO to GND when pressed. Firmware uses internal pull-ups, 30 ms debounce, one action per press, and release between screens. Left/Right browse; Centre confirms setup options. Clearing and recovery use the Pi terminal.

## First full-hardware check

Connect logic with servo power off, flash the full version, and reset ESP32 if PCA was absent at boot. Run the Pi service. Clear board/indexer/feed path, enter `confirm-clear` inside its terminal, then verify the normal menu and buttons. Keep discs out for initial servo and IR qualification. Calibrated command angles and passing software tests do not establish loaded retention, reliable magazine isolation or real sensor timing.
