# Connect Four: ESP32 controller and Raspberry Pi coaching

The firmware in `firmware/ConnectFourGameTest` is the integration version. The original `Arduino/ConnectFourGameTest` remains unchanged. ESP32 owns gameplay, minimax, buttons, OLED, sensing, motor sequencing, faults and move commitment. The Pi is an optional USB sidecar for the operator terminal, game logs, Gemini explanations and ElevenLabs speech.

## Start a game

1. Clear the board, indexer and feed path; enter `confirm-clear` in the Pi terminal.
2. On the OLED, use Left/Right to highlight Free Play or Coach, then press Centre to confirm.
3. Free Play: use Left/Right to browse Easy, Medium and Hard, then Centre to confirm. Next browse Human or Robot first, then Centre to start. Defaults are Free Play, Easy and Human first.
4. Coach starts with the human first and medium robot AI. The first robot response prefers the centre unless an immediate win/block takes priority. Later responses adapt to the board.
5. At `HumanReady`, place a human disc and type its column `1–7` through the Pi terminal. Robot moves still require exactly one qualified target IR passage followed by quiet time and hatch closure before commitment.

Left connects GPIO13 to GND, Right connects GPIO14 to GND, and Centre connects GPIO23 to GND. All three use internal pull-ups. Confirm the actual wiring before powering the prototype. Inputs debounce for 30 ms and act once per stable press; holding does not repeat. Left/Right wrap around at either end of a menu. Release all three buttons between screens; simultaneous presses do not select anything. Centre confirms setup menus only; clearing and recovery retain their terminal commands.

Coach gives short hints and move feedback, combining feedback with the next hint when gameplay advances quickly. Both modes receive an end-of-game review. Speech never blocks gameplay; outdated API results/audio are discarded. No microphone or voice commands are included.

Terminal-only setup also works:

```text
confirm-clear
free
medium
0
```

`0` means human first; `1` means robot first. Alternatively enter `coach` after `confirm-clear`. Use `stop`, `restart`, `correct`, `arm-manual`, and `confirm-correction` as documented in the firmware README. No automatic recovery or feed retry is added.

## API keys

Fill in the blank `pi/.env`, which is excluded from Git:

```dotenv
GEMINI_API_KEY=your_key
ELEVENLABS_API_KEY=your_key
```

`pi/.env.example` lists every supported setting. Model, voice ID, serial port, speech enablement and ALSA audio device are configurable. Gemini defaults to `gemini-3.5-flash-lite`; ElevenLabs defaults to its quickstart voice and `eleven_flash_v2_5`. Use a voice/model available to your account. Do not put credentials in shell commands or commit them.

The Pi computes legal columns, immediate wins, threats, forks and five-ply move scores, then supplies that evidence to Gemini through its structured-output REST API. This finite search does not prove perfect play. If Gemini is unavailable, the terminal uses a local explanation. ElevenLabs creates 24 kHz mono PCM; the sidecar wraps it as WAV for `aplay`. Provider calls have a 10-second deadline; only completed-game reviews retry once. The audio cache holds at most 128 files.

## Pi setup, when ready

These are operator steps; no SSH connection, remote install, firmware upload or Bluetooth pairing has been performed.

1. Enable SSH on the Pi using Raspberry Pi Imager customization or `raspi-config`. Connect from Windows using `ssh <username>@<hostname-or-ip>`; the username is the one you configured, not necessarily `pi`.
2. Copy this implementation's `pi` directory to your chosen Pi project directory. The commands below run from that directory. Keep `.env` private (`chmod 600 .env`).
3. Install the runtime and create its virtual environment:

```bash
sudo apt update
sudo apt install python3-venv python3-pip alsa-utils
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

4. Plug ESP32 into Pi USB. Use `ls -l /dev/serial/by-id/` to find a stable device path and put it in `SERIAL_PORT`. Give your user serial access with `sudo usermod -aG dialout "$USER"`, then log out and back in. Close Arduino Serial Monitor and other serial owners.
5. Initially run without APIs/audio:

```bash
.venv/bin/python -m connect4 --local-only
```

6. Configure Bluetooth audio separately. Raspberry Pi OS Lite needs an audio server for Bluetooth; use PipeWire with its ALSA bridge:

```bash
sudo apt install pipewire pipewire-pulse pipewire-audio pulseaudio-utils pipewire-alsa bluez
sudo reboot
```

After reconnecting over SSH, put the speaker in pairing mode and run `bluetoothctl`. Use `power on`, `agent on`, `default-agent`, `scan on`, then `pair <speaker-MAC>`, `trust <speaker-MAC>`, `connect <speaker-MAC>`, `scan off`, and `quit`. List sinks with `pactl list short sinks`, select the speaker with `pactl set-default-sink <sink-name>`, and unmute it with `pactl set-sink-mute <sink-name> 0`. Keep `AUDIO_DEVICE=default` for the PipeWire ALSA bridge. Verify ordinary WAV playback with `aplay` before enabling API speech. If no sink appears, verify the PipeWire/WirePlumber user services and Bluetooth connection rather than changing the firmware.

7. Run `.venv/bin/python -m connect4 --no-speech` to check Gemini text, then `.venv/bin/python -m connect4` for speech. If SSH drops, ESP32 keeps its current board and sequencing. Reconnect and start the terminal again; do not repeat a physical move merely because its acknowledgement was lost.

The terminal is an interactive foreground program, not an unattended system service. Use `quit` to leave; it does not issue `stop` to ESP32. If you need motion stopped, enter `stop` first. Some USB adapters may reset ESP32 when opened despite DTR/RTS being disabled; verify your board. After an ESP32 reset, clear and confirm the physical board before starting again.

## Logs and recovery

Validated states and deduplicated committed moves are saved in `pi/logs/game-<boot_id>-<game_id>.jsonl`, along with explanations and session status. Pending robot choices are state information, not committed moves. ESP32 retains the current game's 42-move history in RAM and includes it in snapshots every two seconds or on request. A Pi reconnect can reconstruct missed moves; an ESP32 reboot cannot reconstruct a lost board. Games completely missed while the Pi is absent cannot be recovered after ESP32 starts a different game.

The terminal rejects unsynchronized human moves and allows one outstanding move request. If a response is lost, enter `snapshot`: the terminal also queries that request's acceptance status. It never automatically resends a move. Conflicting history or a damaged log disables human input through the Pi until valid state/log storage is restored; preserve the log for inspection.

Replay a saved log without serial access, keys, audio or network requests:

```bash
.venv/bin/python -m connect4 --replay logs/game-42-1.jsonl
```

## Verification

On Windows, from this directory, `cmd /c build-native.cmd` initializes the installed Visual Studio Build Tools environment and runs four native firmware suites. Alternatively run `firmware/ConnectFourGameTest/tests/run-native.ps1` from a developer PowerShell. All outputs stay in `build`.

From `pi`, run `python -m unittest discover -s tests -v`. These tests mock serial and providers, exercise stale speech and history recovery, and validate frames generated by the native C++ serializer when its executable is available. No API credits or hardware are used. On Linux, the Windows executable test is skipped.

Compile the copied sketch using `compile-firmware.ps1`, or Arduino IDE Verify with ESP32 Dev Module. No upload is automatic. Hardware checks still require servo power off/unloaded operation first: verify larger OLED layouts, all three buttons, all seven IR sensors, disconnect/reconnect behavior, and loss of APIs/audio. The existing PCA2–4 isolation and mechanical limitations remain in effect; see the firmware README.

## References

- [Gemini structured responses](https://ai.google.dev/gemini-api/docs/structured-output) and [model catalogue](https://ai.google.dev/gemini-api/docs/models)
- [ElevenLabs create speech API](https://elevenlabs.io/docs/api-reference/text-to-speech/convert)
- [Raspberry Pi remote access](https://www.raspberrypi.com/documentation/computers/remote-access.html) and [audio option guide](https://pip.raspberrypi.com/categories/1259-audio-camera-and-display)
- [Serial protocol](PROTOCOL.md)
