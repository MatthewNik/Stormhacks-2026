# Motor command program

This is a separate ESP32/Pi bench program for checking servo positions in the board. It replaces the ESP32 game firmware while installed; your saved full game code and Pi configuration remain intact. It uses PCA9685 and USB serial only; buttons, OLED and IR inputs are not used. No `confirm-clear`, game selection, API keys or speech service are required.

## Commands inside the motor terminal

```text
servo 0 100
off 0
status
off all
quit
```

`servo <0-6> up` and `servo <0-6> down` use calibrated flap defaults; `servo 7 load` and `servo 7 unload` use 110° and 180°. Presets are resolved by ESP32, so Pi holds no duplicate angle table.

`servo <motor> <angle>` accepts motors 0–7 and whole-number angles 0–180. Every valid command moves only the specified output. The angle is nominal command position, not measured shaft feedback. `status` reports last commanded angles and pulse state. `off` removes pulses/holding torque, not electrical power. There is no automatic disc release or motor movement at startup.

The terminal verifies this specific firmware identity before forwarding motor commands and stops all outputs on a new session. It sends a heartbeat every 500 ms. ESP32 disables pulses if no heartbeat or valid servo command arrives for 2 seconds. `quit`, EOF and Ctrl+C attempt `off all` before closing; USB failure/process loss falls back to the ESP32 heartbeat deadline. Disconnected commands are never automatically replayed. Holding continues while the terminal is alive, so send `off` when each check is done. A hardware controller hang is not covered by the software heartbeat.

## Wiring and known angles

Use the current board wiring. PCA VCC is ESP32 3.3 V logic; SDA GPIO21, SCL GPIO22, OE GPIO25 with external 10 kOhm pull-up to 3.3 V, address 0x40. PCA GND, ESP32 GND and external servo supply negative are common. Servo red wires use separate regulated V+; orange/yellow use channel S, brown/black use GND. Do not route servo current through ESP32 or breadboard logic rails. Start within the servos' rated range, such as 4.8 V, and do not join the servo positive rail to logic power.

Motor numbers are printed PCA connector numbers, not one-based column numbers:

| Motor/PCA | Column from front | Down/closed | Up/open |
| --- | --- | --- | --- |
| 0 | 1, rightmost | 15° | 115° |
| 1 | 2 | 8° | 110° |
| 2 | 3 | 11° | 100° |
| 3 | 4, centre | 12° | 105° |
| 4 | 5 | 11° | 100° |
| 5 | 6 | 12° | 115° |
| 6 | 7, leftmost | 12° | 110° |

Motor 7 loads the magazine at 110° and unloads/releases at 180°. Outputs 8–15 remain FULL_OFF. Pulse mapping is the same nominal 500–2500 us range used by the Uno calibration and full ESP32 firmware, at approximately 50 Hz with prescaler-aware conversion.

Remove discs for testing and command one motor at a time. Begin near a known safe position and avoid linkage/hard-stop binding. Multiple commands can leave multiple motors holding; this diagnostic does not run the full game's flap timing or IR safety sequence. Motor 7 is directly commandable, so its mechanism/feed path must be empty before testing.

## Stop the game first

If the full Pi Connect Four terminal is running, enter `stop`, then `quit`, inside that terminal. Close any other serial owner. Keep external servo power off during flashing and power disconnected when changing wiring. PCA9685 must be connected; an initialization or later communication/configuration fault disables OE and requires repair plus ESP32 reset.

## Upload from Windows PowerShell

The verified artifacts are in `deployment`. Replace the SSH host using the same hostname/IP as your usual connection:

```powershell
Set-Location 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4'
$PiHost = Read-Host 'Enter the Pi hostname or IP you use for SSH'
ssh "matthew@$PiHost" 'mkdir -p ~/stormhacks-motor-test/update'
scp 'motor command/deployment/firmware.bin' 'motor command/deployment/pi-update.zip' 'motor command/deployment/SHA256SUMS' "matthew@${PiHost}:~/stormhacks-motor-test/update/"
ssh "matthew@$PiHost"
```

## Install and flash in Pi SSH

Run in order and stop on any error. Both checksum lines must report OK before continuing:

```bash
cd ~/stormhacks-motor-test/update
sha256sum -c SHA256SUMS
python3 -m zipfile -e pi-update.zip ..
cd ~/stormhacks-motor-test
python3 -m venv .venv
.venv/bin/python -m pip install --timeout 120 --retries 10 -r requirements.txt
ls -l /dev/serial/by-id/
~/stormhacks-rpi/.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 update/firmware.bin
```

This uses the existing esptool environment in `~/stormhacks-rpi/.flash-venv`. If installation is still incomplete:

```bash
python3 -m venv ~/stormhacks-rpi/.flash-venv
~/stormhacks-rpi/.flash-venv/bin/python -m pip install --timeout 120 --retries 10 'esptool>=5,<6'
```

The USB path is the CP2102 device recorded in the handoff; use the actual path if the listing differs. Flashing resets ESP32 and replaces its firmware/flash contents. If automatic reset does not connect, hold BOOT, tap EN, and release BOOT when connected. Wait for successful write verification before powering servos.

## Run and command a motor

With the magazine/board empty and wiring checked, enable the correctly rated servo supply and run:

```bash
cd ~/stormhacks-motor-test
.venv/bin/python motor_terminal.py
```

It should identify `MOTOR_TEST v1 READY` and print the command list. Enter `servo 0 100` inside this program, not Bash. Use `off 0` after checking and `quit` to finish. A different serial path can be supplied with `--port /dev/serial/by-id/...`. A fault indication means repair PCA wiring and reset ESP32; the Pi cannot bypass that fault.

## Return to full gameplay

Quit this motor terminal and keep servo power off while flashing. Restore the verified full game firmware following [the full deployment guide](../working%20code/DEPLOY.md). If the full package is already on the Pi at its documented location:

```bash
cd ~/stormhacks-rpi
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 full-update/firmware.bin
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4
```

The updated full package opens Mode automatically on healthy boot. Restart/recovery clearing remains explicit.

## Local rebuild and verification

Run `compile-firmware.ps1` followed by `package-deployment.ps1` from this directory. `build-native.cmd` tests ESP32 parsing, mapping, off/fault behavior, heartbeat expiry and timer wraparound. From `pi`, `python -m unittest discover -s tests -v` tests command validation, firmware identification, heartbeats, stopped sessions and no replay. Build, test, temporary and package outputs stay here.

References: [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html), [Espressif flashing](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html).
