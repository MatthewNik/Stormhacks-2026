# Install the full IR game on Pi and ESP32

Use Pi user `matthew`, directory `~/stormhacks-rpi`, and a classic ESP32 Dev Module. The packages are `deployment/firmware.bin` (merged image), `pi-update.zip`, and `SHA256SUMS`. This installation has no APIs, speech or coaching; Coach is an unavailable OLED option.

## Stop and prepare

In the running Pi program enter `stop`, then `quit`. Switch external servo power off; leave ESP32 connected to Pi USB and close other serial owners. Flashing resets the logical board/history. Do not resume an existing physical game after flashing. Existing remote `.env`, virtual environments, logs and Bluetooth settings are preserved by the source package.

## Windows PowerShell

The saved deployment packages are ready after successful validation. To rebuild them locally, run:

```powershell
Set-Location 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4'
& '.\working code\compile-firmware.ps1'
& '.\working code\package-deployment.ps1'
```

Stop if any command fails. Transfer the packages:

```powershell
Set-Location 'C:\Users\matth\Documents\Stormhacks2026 Oct 3-4'
$PiHost = Read-Host 'Pi hostname or IP'
ssh "matthew@$PiHost" 'mkdir -p ~/stormhacks-rpi/full-update'
scp 'working code/deployment/firmware.bin' 'working code/deployment/pi-update.zip' 'working code/deployment/SHA256SUMS' "matthew@${PiHost}:~/stormhacks-rpi/full-update/"
ssh "matthew@$PiHost"
```

## Pi dependency setup

On a fresh Pi OS installation, run:

```bash
set -e
sudo apt update
sudo apt install -y python3-venv python3-pip
sudo usermod -aG dialout matthew
exit
```

Reconnect from Windows so group membership takes effect:

```powershell
ssh "matthew@$PiHost"
```

## Install and flash in Pi SSH

Keep servo power off. Both checksum entries must report OK. Stop on errors:

```bash
set -e
cd ~/stormhacks-rpi/full-update
sha256sum -c SHA256SUMS
python3 -m zipfile -e pi-update.zip ../pi

cd ~/stormhacks-rpi/pi
python3 -m venv .venv
.venv/bin/python -m pip install --timeout 120 --retries 10 -r requirements.txt

cd ~/stormhacks-rpi
python3 -m venv .flash-venv
.flash-venv/bin/python -m pip install --timeout 120 --retries 10 'esptool>=5,<6'
ls -l /dev/serial/by-id/
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 full-update/firmware.bin
```

If the listing shows a different serial path, substitute it in flashing and startup. If automatic connection fails, hold BOOT, tap EN and release BOOT when the uploader connects. See [Espressif flashing commands](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html).

## Start and check

Connect hardware according to [WIRING.md](WIRING.md); disconnect USB and external power before rewiring. PCA9685 must be present at boot or ESP32 latches Fault. Physically clear the board/indexer/feed path and keep discs out for the first motion check.

With servo power off, run:

```bash
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4 --serial-port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

Enable servo power to check commanded startup positions: PCA0–6 up/open at 115/110/100/105/100/115/110°, PCA7 load 110°. All position signals remain active. When the OLED reaches Mode, browse Free Play/Coach with Left/Right; Coach shows Unavailable. Centre confirms Free Play, difficulty, then starter. Wait for **Insert ONE disc** before human insertion. IR registers the column; do not type it.

For unloaded robot sequencing, the engine-selected door stays open while the others close; PCA7 commands release 180°. With no IR passage this intentionally pauses after timeout, without repeating a release. Use restart and physical clearing before another test. Qualify all real sensor passages and magazine isolation before loaded play.

Healthy boot needs no initial `confirm-clear`. Explicit restart does: `restart`, physically clear, then `confirm-clear`. `stop` disables all motor outputs; `quit` alone leaves ESP32 running. Serial reopen may reset the controller, automatically repositioning motors and starting an empty logical game.

OLED shows only setup menus and plain turn/status text; board diagrams remain in the Pi terminal. If PCA Fault appears, enter `diagnose` in the running Pi program and retain the `PCA detail:` line. That line distinguishes address/communication errors from initialization/register/configuration/write failures. It is also repeated automatically in Fault snapshots, and diagnosis does not clear the fault or move motors.

Build 019 samples the IR inputs from a 250 us hardware timer instead of per-edge GPIO interrupts, starts door commands 150 ms apart, and retries a failed PCA register read or write up to three times before faulting. A `WARNING: PCA I2C retried N time(s)` line means the bus was disturbed but recovered; a `PCA reset detected` detail means the PCA chip itself lost power. Update both firmware and Pi source. The terminal should print `Connect Four build 019: timer-sampled IR; staggered hatches; PCA I2C retries; no APIs.` after an ESP32 reset. On an already running ESP32 this startup line may have passed before the connection, but `snapshot` and `diagnose` remain available. A USB connection with no game snapshot for six seconds now produces a notice; opening a serial port does not prove that game firmware is responding.
