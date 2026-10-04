# Standalone motor command program

The user requested a separate program to send `servo 0 100` through Pi SSH and check motor positions in the board. Use [motor command](../motor%20command/README.md). Full game source, button-test source, and original integration source remain unchanged.

## Firmware and terminal

ESP32 firmware is `motor command/firmware/MotorCommandTest`. It controls PCA9685 at 0x40 via SDA21/SCL22, OE25 with external pull-up, and 50 Hz prescaler-aware 500–2500 us angle mapping. Only connectors 0–7 accept commands; 8–15 stay FULL_OFF. Startup initializes outputs off and performs no motor movement. PCA initialization/write/prescale/communication faults disable OE and require repair/reset.

Pi's separate `motor_terminal.py` forwards validated plain-text commands over USB at 115200 baud. Commands: `servo <0-7> <0-180>`, `off <0-7>`, `off all`, `status`, `help`, `quit`. `status` shows commanded angles/pulse state, not position feedback. It verifies `MOTOR_TEST v1 READY` or `FAULT` before motor forwarding and sends `off all` on a new session. It sends a 500 ms heartbeat; ESP32 removes all pulses after 2 seconds without contact. Pulses otherwise remain active until off/quit. Normal quit/EOF/Ctrl+C attempts to stop outputs; lost link relies on heartbeat expiry. There is no automatic reconnect or command replay.

This diagnostic replaces full gameplay firmware while installed. It does not operate OLED, buttons, IR, AI, API services, automatic hatch batches, or magazine safety sequencing. Motor 7 is directly commandable; empty the board and feed path for tests. Test one motor at a time. Off removes holding torque, not supply voltage; physical retention and hardware controller hangs are outside software heartbeat guarantees.

## Deployment

Verified files are `motor command/deployment/firmware.bin`, `pi-update.zip`, and `SHA256SUMS`. The Pi archive contains only `motor_terminal.py` and `requirements.txt` (pyserial 3.5). Install separately under `~/stormhacks-motor-test`; leave `~/stormhacks-rpi/pi` and its `.env`, logs and audio configuration intact. Stop/quit the full Pi program before opening the motor terminal or flashing. Use the existing `~/stormhacks-rpi/.flash-venv` to flash the USB-connected ESP32 with external servo power off. Exact PowerShell transfer, SSH install/flash/run, and full-firmware restore commands are in the README.

Motor numbers are PCA connector numbers. PCA0 is front-right column 1; PCA6 is front-left column 7. Known closed/open angles remain 50/140, 60/145, 55/140, 55/147, 55/140, 55/141, 70/140. Motor 7 loads at 80 and unloads at 145 degrees.

## Verification and pending work

The firmware native suite passed parsing/range rejection, pulse mapping, overlong/control-character line handling, startup/off/fault behavior, heartbeat deadline, one-channel off, and timer wraparound. Five Pi tests passed validation, identity handshake, fragmented input, timed heartbeat, wrong-firmware rejection, partial-write failure and stopped sessions. ESP32 compile passed with 301028 bytes flash and 23764 bytes RAM; deployment binary matches the build, Pi archive and SHA256 manifest were verified. No device upload or physical motor motion was tested here.

Handoff 011's automatic full-game Mode-menu startup remains pending for the next full-game update. This standalone diagnostic requires no `confirm-clear` and does not modify that game's startup policy.
