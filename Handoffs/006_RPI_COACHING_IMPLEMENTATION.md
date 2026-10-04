# Raspberry Pi coaching integration

## Entry points

Work only in `C:\Users\matth\Documents\Stormhacks2026 Oct 3-4`. The original `Arduino/ConnectFourGameTest` and standalone hardware tests are unchanged. The integration version is in [RPI IMPLEMENTATION](../RPI%20IMPLEMENTATION/README.md), with firmware at `firmware/ConnectFourGameTest` and a Python sidecar at `pi/connect4`. Read the [USB protocol](../RPI%20IMPLEMENTATION/PROTOCOL.md) before changing either end.

## Implemented behavior

- ESP32 remains authoritative for board, minimax, buttons/OLED, sensors, actuator sequencing, faults and commitment. The sensor/recovery/output behavior from handoff 005 is preserved, including PCA2–4 isolation and unassembled-bench limitations.
- After `confirm-clear`, button 1 selects Free Play and button 2 selects Coach. Free Play difficulty: press 1 Easy, hold 1 Medium, press 2 Hard, hold 2 Confirm. Starter: press 1 Human, hold 1 Robot, press 2 Confirm. GPIO13/14 to GND with internal pull-ups; actual wiring still needs checking. Debounce 30 ms, stable hold 700 ms; release between screens; simultaneous presses suppressed.
- Separate 96x64 OLED menus use size-2 selected text and size-1 instructions. Gameplay keeps the compact board display.
- Coach is human-first, medium AI, centre-preferred first robot response unless an immediate tactic takes priority, then adaptive normal AI. Human IR input remains disabled. Human columns are still manually entered after physical placement, through the Pi SSH terminal.
- USB output is versioned JSON with full board, phase, pending target and current-game committed history. Commands use the documented bounded ASCII grammar. Human requests carry boot/game/turn/request IDs; stale/duplicate requests cannot commit another move. Robot commitment still requires qualified target IR, quiet time and completed closure.
- Pi has one reconnecting serial owner and an interactive terminal. It never automatically replays moves or recovery commands. Validated snapshots reconstruct missed committed moves; logs are deduplicated and retained across Pi process restarts. ESP32 reset loses RAM history and requires clearing confirmation.
- Pi computes local tactical evidence and depth-5 comparisons. Gemini explains that evidence through structured-output REST; ElevenLabs produces PCM wrapped as WAV and played by `aplay`. Separate workers replace pending advice and cancel stale speech. Provider deadlines are 10 seconds; only completed-game reviews retry once. Local coaching text is available without Gemini.
- Coach provides hints and feedback; Free Play speaks only an end review. No microphones, voice commands, dashboards or unattended service are included.

## Configuration and setup

`RPI IMPLEMENTATION/pi/.env` exists with blank keys and is ignored by Git. Fill `GEMINI_API_KEY` and `ELEVENLABS_API_KEY`; configurable model/voice/serial/audio settings are listed in `.env.example`. Do not print or commit keys. No API calls were made during verification.

The integration README contains deferred SSH, Python virtual environment, serial permissions and PipeWire Bluetooth-audio instructions. Verify the Pi OS version, serial adapter and speaker when setting up. Use a stable `/dev/serial/by-id/` path. Run `python -m connect4 --local-only` first, then Gemini with `--no-speech`, then full speech. `quit` exits the terminal without stopping ESP32; use `stop` first if required.

## Verification

- Four native firmware suites passed: existing game/controller/AI/output sequencing, PCA adapter, production ISR capture adapter, and new menus/protocol/history/telemetry/coach tests.
- ESP32 Dev Module compile passed: **338392 bytes flash**, **33868 bytes global RAM**. ESP32 core 3.3.12 with the existing Adafruit dependencies and bundled SSD1331 driver. Outputs are under `RPI IMPLEMENTATION/build/esp32`; no upload occurred.
- All **19 Python tests passed**, covering board tactics/gravity/results, history conflicts/reconnect/reset/deduplication, provider constraints/deadlines/sanitized errors, terminal turn gating, serial fragmentation/overflow, stale API/audio work and bounded review retries. A cross-language test parses actual C++ frames through the Python validator.
- Native runner: `RPI IMPLEMENTATION/build-native.cmd`, or the copied `tests/run-native.ps1` in a developer PowerShell. Python runner: from `RPI IMPLEMENTATION/pi`, `python -m unittest discover -s tests -v`. Build/test/cache outputs stay within the integration directory and are ignored.

## Next checks

1. Configure SSH and deploy the Pi directory when ready; no SSH, remote install or Bluetooth pairing has occurred.
2. With servo power off, verify all OLED menus and GPIO13/14 button wiring, presses, holds and release between screens.
3. Verify plain Bluetooth WAV playback before enabling ElevenLabs. Check Gemini model and ElevenLabs voice access with the supplied account keys.
4. Perform unloaded serial/gameplay/sensor/recovery tests. Test USB reconnect, lost internet and speaker disconnect; none may invent a commitment or issue a second feed release.
5. Continue the physical checks from handoff 005 before any loaded run. The implementation has not established mechanical retention, one-disc isolation, actual servo travel, interrupt latency or sensor qualification on hardware.
