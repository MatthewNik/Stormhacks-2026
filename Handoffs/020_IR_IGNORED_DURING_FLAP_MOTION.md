# Handoff 020: IR ignored during flap motion (build 020)

Branch: `danny-test`. Firmware-only; the Pi `connect4` program is unchanged.

## Result of build 019 on hardware

The human move registered (column 4 committed), so the debounced sampler fixed the "Sensor backlog or overflow" pause. The robot turn then paused in `RobotOpening` with "Premature or extra sensor detection": a sensor qualified as a disc while the non-target flaps were swinging closed. TCRT5000s are reflective, so a flap sweeping through (or stopping in) a sensor's view looks like a puck.

## What changed

- `RobotOpening`: IR is disarmed while flaps move, then re-armed when motion settles; `RobotBaseline` still requires 100 ms stable clear before the indexer releases.
- `RobotClosing`: IR is disarmed while the target flap closes, re-armed after, and commitment needs a fresh 100 ms stable-clear seal. Trade-off: a disc dropped during the ~650 ms closing motion is not observed. The manual-recovery path waits for that seal before prompting `confirm-correction`.
- Sensor pause messages now name the column and phase, e.g. `Sensor stuck active (IR column 6, phase RobotBaseline)`. A flap that stops in a sensor's view now reports as stuck on that column instead of "Premature or extra".
- New `testFlapMotionIgnored` in `native_tests.cpp`; all five native suites pass. The ESP32 build compiles with core 3.3.12. **Not yet run on hardware.**

## Rebuild and flash (from Danny's PC, then the Pi SSH session)

1. On the PC, build (PowerShell):

```powershell
$Cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
cd "C:\Github\Stormhacks-2026\working code"
& $Cli compile --fqbn esp32:esp32:esp32 --build-path "$PWD\build\esp32" --build-property "compiler.cpp.extra_flags=-DMENU_TEST_ONLY=0" firmware\ConnectFourGameTest
(Get-FileHash build\esp32\ConnectFourGameTest.ino.merged.bin -Algorithm SHA256).Hash.ToLower()
cd build\esp32
python -m http.server 8000
```

2. In the Pi SSH session: in the running game enter `stop`, then `quit`, and switch servo power off. Replace the IP with the PC's current address (`ipconfig` on the PC):

```bash
cd ~/stormhacks-rpi
curl -fo full-update/firmware.bin http://10.185.92.86:8000/ConnectFourGameTest.ino.merged.bin
sha256sum full-update/firmware.bin
pgrep -af connect4
.flash-venv/bin/python -m esptool --chip esp32 --port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0 --baud 115200 write-flash 0x0 full-update/firmware.bin
cd pi
.venv/bin/python -m connect4 --serial-port /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

The hash must match step 1 and `pgrep` must print nothing before flashing. Stop the PC web server with Ctrl+C afterwards. Expect `Connect Four build 020: IR ignored during flap motion; debounced sampler; tolerant PCA check.`

## What to look for

- Robot turn should now reach `IndexerLoading` and release.
- If it pauses with `Sensor stuck active (IR column N, phase RobotBaseline)`, a closed flap sits in column N's sensor view: re-aim or shroud that sensor.
- Any other sensor pause now names the column; report the full line.
