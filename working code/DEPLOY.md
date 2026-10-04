# Update the Pi and ESP32

These commands use the existing Pi user `matthew`, deployment directory `~/stormhacks-rpi`, and CP2102 USB serial path recorded in handoff 008. Use the actual hostname/IP you already use for SSH. This upload targets the classic ESP32 Dev Module.

## Stop the current program

In the running Connect Four terminal, enter these separately:

```text
stop
quit
```

If it is not running, skip this step. Leave ESP32 connected to Pi USB; close all other serial owners. Switch external servo power off. Flashing resets ESP32 and clears RAM game history. The merged image also replaces the flash image and may replace stored flash settings; do not resume a physical game after upload. The Pi's `.env` and audio configuration are not included or overwritten.

## Send the full version from Windows PowerShell

```powershell
Set-Location 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4'
$PiHost = Read-Host 'Enter the Pi hostname or IP you use for SSH'
ssh "matthew@$PiHost" 'mkdir -p ~/stormhacks-rpi/full-update'
scp 'working code/deployment/firmware.bin' 'working code/deployment/pi-update.zip' 'working code/deployment/SHA256SUMS' "matthew@${PiHost}:~/stormhacks-rpi/full-update/"
ssh "matthew@$PiHost"
```

## Apply the full version in the Pi SSH shell

Run each step in order. Stop if a command fails. Both checksum checks must report OK before extraction or flashing.

```bash
cd ~/stormhacks-rpi/full-update
sha256sum -c SHA256SUMS
python3 -m zipfile -e pi-update.zip ../pi
cd ~/stormhacks-rpi/pi
.venv/bin/python -m pip install --timeout 120 --retries 10 -r requirements.txt
cd ~/stormhacks-rpi
.flash-venv/bin/python -m esptool version
ls -l /dev/serial/by-id/
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 full-update/firmware.bin
```

If `esptool version` reports a missing module, finish installation before flashing:

```bash
python3 -m venv .flash-venv
.flash-venv/bin/python -m pip install --timeout 120 --retries 10 'esptool>=5,<6'
```

If the serial listing shows a different path, use that actual path. If upload cannot connect automatically, hold BOOT, tap EN, and release BOOT when the uploader connects. Leave servo power off during flashing.

## Start the Pi program

With USB and external power disconnected, connect PCA9685 logic/OE and the OLED/buttons/sensors following the firmware README. The full version requires PCA9685; absence causes a latched fault that requires connecting it and resetting ESP32. Start with external servo power off, reconnect logic power, and verify the display/buttons first. Clear the physical board, magazine indexer and feed path before acknowledging clearing.

For the first check without API calls or speech:

```bash
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4 --local-only
```

Inside the running Connect Four terminal (not Bash), enter:

```text
confirm-clear
```

The OLED should now show Mode without the TEST label. Left/GPIO13 and Right/GPIO14 browse; Centre/GPIO23 confirms. Each button shorts its GPIO to GND. Release between presses. Use unloaded servo checks before any discs; do not confirm a start until ready for motion.

After stopping with `stop`, then `quit`, run the full API/audio service using the existing `.env`:

```bash
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4
```

A serial reopen can reset the ESP32; only enter `confirm-clear` after physically checking it again if requested.

## Local rebuilds

From this full-hardware directory, run `compile-firmware.ps1` followed by `package-deployment.ps1`. No switch is needed: this build script always compiles the full hardware version with MENU_TEST_ONLY=0. Build and package errors must be resolved before copying artifacts. Packages include checksums for transfer verification. Build outputs, temporary files and deployment artifacts stay inside each directory.

Sources: [Espressif flashing guide](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html), [merged firmware images](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html).
