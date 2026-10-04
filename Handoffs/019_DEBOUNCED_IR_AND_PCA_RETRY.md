# Handoff 019: Debounced IR sampler and tolerant PCA health check (build 019)

Branch: `danny-test`. Firmware-only change; the Pi `connect4` program is unchanged, so only the ESP32 needs reflashing.

## What changed and why

- **IR "Sensor backlog or overflow".** The DAOKI TCRT5000 boards use an LM393 with no hysteresis, so a slow crossing of the threshold (a hand-held drop, a puck near the 25–45 mm range edge) chatters hundreds of edges. The old `CHANGE` ISR queued every edge into 64 slots, and the ~100 ms OLED redraw on `HumanReady → HumanConfirm` stopped the loop from draining it mid-passage. `Esp32Sensors.h` now samples all 7 inputs every 500 µs from an `esp_timer` (core 0, independent of OLED/AI), requires 2 ms of stable level, and queues only debounced transitions. No GPIO interrupts are attached. `SensorService` is unchanged.
- **PCA faults.** One failed health read used to latch Fault forever. `PcaHardware::monitor()` now faults only after 3 consecutive misses (re-checking every 20 ms after a miss, so a dead/reset PCA still faults within ~40 ms). PWM register writes are retried once (absolute writes, safe to repeat). `diagnose` now reports the count and last detail of recovered misses: frequent misses mean bus noise / ground bounce worth fixing in hardware.
- New constants in `GameConfig.h`: `SENSOR_SAMPLE_US`, `SENSOR_STABLE_SAMPLES`, `BUS_RETRY_MS`, `PCA_HEALTH_MISSES`.
- Tests: `sensor_capture_tests.cpp` rewritten for the sampler (chatter rejection, stalled-loop passage); `pca_hardware_tests.cpp` gains retry/miss-threshold cases. All five native suites pass. **Not yet built with the ESP32 core or run on hardware.**

## Build on the Windows PC

The existing `compile-firmware.ps1` / `package-deployment.ps1` refuse to run outside `C:\Users\matth\Documents\Stormhacks2026 Oct 3-4`. The commands below work from any clone. They assume the ESP32 core and the Adafruit PWM Servo Driver / Adafruit GFX libraries already installed for the earlier builds.

```powershell
git fetch origin
git switch danny-test
git pull
Set-Location 'working code'
$Cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
& $Cli compile --fqbn esp32:esp32:esp32 --libraries "$HOME\Documents\Arduino\libraries" --build-path "$PWD\build\esp32" --build-property "compiler.cpp.extra_flags=-DMENU_TEST_ONLY=0" firmware\ConnectFourGameTest
```

Stop if the compile fails. The flashable image is `build\esp32\ConnectFourGameTest.ino.merged.bin`.

Optional, native tests (from a *Developer PowerShell for VS*, in `working code\firmware\ConnectFourGameTest\tests`). Do not define `NDEBUG`: the tests call functions inside `assert`.

```powershell
foreach ($n in 'native_tests','pca_hardware_tests','sensor_capture_tests','sidecar_tests','menu_tests') {
  $f = if ($n -eq 'menu_tests') { @() } else { @('/DMENU_TEST_ONLY=0') }
  cl @f /nologo /std:c++17 /EHsc /W4 /I stubs "$n.cpp" "/Fe$env:TEMP\$n.exe" "/Fo$env:TEMP\$n.obj"; & "$env:TEMP\$n.exe"
}
```

## Copy to the Pi

```powershell
$PiHost = Read-Host 'Pi hostname or IP'
ssh "matthew@$PiHost" 'mkdir -p ~/stormhacks-rpi/full-update'
scp build\esp32\ConnectFourGameTest.ino.merged.bin "matthew@${PiHost}:~/stormhacks-rpi/full-update/firmware.bin"
(Get-FileHash build\esp32\ConnectFourGameTest.ino.merged.bin -Algorithm SHA256).Hash.ToLower()
ssh "matthew@$PiHost"
```

Note the printed hash for the check below.

## Flash from the Pi SSH session

First, in the running Pi program enter `stop`, then `quit` (it owns the serial port). Switch servo power off. Flashing resets the logical game.

```bash
cd ~/stormhacks-rpi
sha256sum full-update/firmware.bin   # must match the hash printed on Windows
ls -l /dev/serial/by-id/
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 full-update/firmware.bin
```

Substitute the serial path if `ls` shows a different one. If `.flash-venv` is missing, create it first: `python3 -m venv .flash-venv && .flash-venv/bin/python -m pip install 'esptool>=5,<6'`. If the uploader cannot connect, hold BOOT, tap EN, release BOOT when it connects.

## Start and check

```bash
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4 --serial-port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

After an ESP32 reset the terminal should print `Connect Four build 019: debounced IR sampler; tolerant PCA health check; no APIs.` An `ERROR: IR sampler timer failed to start` line means the sensors are not running; report it.

Hardware checks, feed path empty first:

1. Human turn: slowly hand-drop one disc per column, including hovering a hand over the sensor first. Each should register once with no `Sensor backlog or overflow`.
2. Run `diagnose` after a few turns even if nothing faulted. `Health-check misses this boot=N` above zero means I2C is glitching; note N and the `last recovered` detail.
3. If a PCA Fault still occurs, keep the full `PCA detail:` line. A `code=` value means a bus error (noise, ground bounce); `PWM prescaler changed` or `sleep mode` means the PCA itself reset (brownout on its supply).

The sensor-range and hysteresis problem remains in hardware; this build stops it from aborting the game, but a shroud or LM393 hysteresis resistor is still worthwhile.
