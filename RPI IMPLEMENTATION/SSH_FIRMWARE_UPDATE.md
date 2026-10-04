# Upload ESP32 firmware through the Pi over SSH

The Pi can flash its USB-connected ESP32. Compile on the laptop and transfer the binary; the Pi does not need the Arduino IDE or ESP32 compiler. This procedure targets the project's classic ESP32 Dev Module, not an ESP32-S3/C3 or another board.

## Prepare

Remove discs and switch off external servo power. Leave ESP32 USB connected to the Pi. In the running Connect Four terminal, enter `stop`, then `quit`. If managed as a service, stop that service too. Close any other serial monitor. Uploading resets ESP32 and loses its RAM game history; do not resume an unfinished physical game afterward.

Build using `compile-firmware.ps1` in this folder. Only transfer the binary after a successful build. The merged image includes bootloader, partitions and application, and may replace persistent flash data/settings; preserve anything needed first. It does not modify the Pi's `.env`.

## Copy from the laptop

In PowerShell, from the active project directory, replace `PI_HOST` with the same hostname/IP you use for SSH:

```powershell
scp "RPI IMPLEMENTATION/build/esp32/ConnectFourGameTest.ino.merged.bin" matthew@PI_HOST:~/stormhacks-rpi/ConnectFourGameTest.ino.merged.bin
ssh matthew@PI_HOST
```

## Flash from the Pi SSH shell

Install the flashing tool in a separate environment once:

```bash
cd ~/stormhacks-rpi
python3 -m venv .flash-venv
.flash-venv/bin/python -m pip install 'esptool>=5,<6'
```

Confirm the serial device is present:

```bash
ls -l /dev/serial/by-id/
```

The previously observed device is the CP2102 path below. If the listed path differs, use the actual one. With the Connect Four service stopped and servo power off:

```bash
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 ConnectFourGameTest.ino.merged.bin
```

Wait for successful write verification and reset before restarting the Pi application. A permission error may require adding the Pi user to `dialout` and reconnecting the SSH session. If automatic reset cannot connect, hold ESP32 BOOT while flashing starts, tap EN, and release BOOT once connected. Do not enable servo power during this process.

After upload, run the normal Pi service, physically clear the board and feed path, and enter `confirm-clear` only after checking them. Verify the updated hatch angles unloaded before adding discs.

This procedure has not been run on the actual Pi or ESP32 during this change.

References: [Espressif flashing guide](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html), [esptool binary offsets and merged images](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html).
