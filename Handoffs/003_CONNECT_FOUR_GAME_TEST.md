# Connect Four game test handoff

New integrated bench simulation: [ConnectFourGameTest](../Arduino/ConnectFourGameTest/README.md). Existing bench sketches are preserved. Hardware pins follow [the successful wiring record](002_SUCCESSFUL_WIRING.md), including the 96x64 SPI SSD1331.

The sketch supports first-player/difficulty selection, fixed O/X assignments, typed columns, ASCII/OLED boards, minimax at 2/4/5 plies, sequenced seven-column hatches and stop/restart. Channels 7-15 remain disabled. PCA faults disable OE and require ESP32 reset. ESP32 owns all game state and commitment; no sensors, magazines, Pi or network are used.

SSD1331 driver 1.3.0 is bundled in the sketch's `src/ssd1331` folder, with its upstream license. This resolves Arduino IDE's missing `Adafruit_SSD1331.h` error without requiring a separate SSD1331 installation. GFX, BusIO and PWM Servo Driver still use their installed libraries.

ESP32 Dev Module compilation of the current bounded-output version passed: 317140 bytes flash, 23996 bytes global RAM. Both native suites passed, covering game/AI/parser/controller sequencing, pulse expiry, isolated outputs and the PCA hardware adapter with simulated communication failures. Build and test outputs are in `tmp/connect-four-game-test`; README contains rerun instructions.

No upload was performed. The previously uploaded ColorOledSpiTest remains the last recorded hardware firmware. The integrated OLED layout, servo motion, endpoints and timing still need physical verification; servo movement was not established by the earlier PCA response.

Next: follow the README's unloaded-servo hardware checklist, verify the OLED menus/numbered board, and validate per-channel calibration. The one-second robot commitment is simulation-only; physical gameplay requires sensor-confirmed commitment and recovery.

The operator subsequently reported repeated full revolutions on **printed PCA connectors 2-4**, believed to be 180-degree units; exact models remain unconfirmed. Those three connectors are temporarily isolated via `HATCH_ENABLED` in `GameConfig.h`, with attempted position writes converted to FULL_OFF. Other channels remain available; typed game simulation includes all columns. Bench pulse cutoffs are scheduled after 300 ms for ordinary commands or 1600 ms for the selected robot opening, serviced on the next loop tick. They do not provide independent protection against an ESP32 hang. New sequences clear stale commands before OE enabling; pulse cutoff failures latch faults. Releasing pulses removes holding torque, so loaded hatch operation is not validated. This mitigates indefinite commands but does not establish a physical repair. Identify continuous-rotation versus positional units and check feedback/mechanics before reconnecting affected servo power or re-enabling a channel.
