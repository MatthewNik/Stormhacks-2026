# Saved button test and full hardware version

Work remains inside the active Oct 3–4 workspace. `RPI IMPLEMENTATION` is preserved; the two new saved versions are independent copies of its current source.

## Current entry points

- [button test](../button%20test/README.md): defaults to menu-only in both Arduino IDE and its compile script. Starts at `TEST: Mode`, substitutes a no-output motor interface, leaves OE HIGH, and does not initialize sensor interrupts. Buttons browse Free Play, difficulty and starter selections. Confirming a starter or Coach returns to Mode; gameplay cannot start.
- [working code](../working%20code/README.md): defaults to full hardware firmware, including normal PCA fault handling and physical clearing confirmation. Missing PCA9685 causes a fault; connecting it requires ESP32 reset before clearing confirmation.
- Both versions include `DEPLOY.md` with exact PowerShell transfer, Pi SSH installation/flashing, and button-test restore commands. `deployment/firmware.bin`, `deployment/pi-update.zip`, and `deployment/SHA256SUMS` are ready.

## Hardware configuration

Left GPIO13, Right GPIO14, Centre GPIO23, each active LOW to GND with internal pull-up. Release between presses. OLED remains the existing 96x64 SSD1331 SPI display.

PCA0/column 1 is front-right; PCA6/column 7 is front-left. Enter human columns and arrange IR sensor wiring in this same order. Up means open; down means closed. Closed/open angles are 50/140, 60/145, 55/140, 55/147, 55/140, 55/141, 70/140 for PCA0–6. All seven enabled following operator confirmation of unloaded angle tests. PCA7 loads at 80 and releases at 145 degrees. Pulse mapping remains 500–2500 us.

Flap batches command PCA0–6 with at least 50 ms between starts; delayed ticks do not issue catch-up bursts. Robot delivery closes and settles all flaps, opens the selected target, then runs the independent magazine sequence. PCA7 is never part of a flap batch. Existing pulse expiry, sensor qualification, fault/recovery handling and ESP32 move ownership remain in effect.

## Verification

Both saved copies passed all five native firmware suites and all 19 Pi tests, including the native serializer/Python contract check. Button build: 333484 bytes flash, 33364 bytes RAM. Full build: 338356 bytes flash, 33876 bytes RAM. Both deployment binaries match their successful builds. SHA256 manifests and both Pi archives were verified.

Source copies and deployment archives exclude `.env`, virtual environments, logs, audio caches and temporary build products. Pi archives contain only Python source and requirements; deploying preserves the Pi's existing credentials and audio configuration. Build scripts, cache configuration and test output paths are isolated to each saved directory.

No SSH connection or device upload was performed during this save/package task. Full hardware motion, loaded retention, sensor timing and dispensing remain unverified. Switch servo power off during flashing, restart the Pi terminal afterward, physically clear board/indexer/feed path and acknowledge `confirm-clear` only after inspection. The merged image replaces flash contents; do not resume an unfinished physical game after ESP32 reset.
