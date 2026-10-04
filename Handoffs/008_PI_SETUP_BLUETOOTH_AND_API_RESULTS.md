# Pi setup, Bluetooth fixes and API test results

## Current workspace and entry points

Continue only in `C:\Users\matth\Documents\Stormhacks2026 Oct 3-4`. The older Stormhacks2026 directory remains reference-only. Use the integration firmware and Pi service in [RPI IMPLEMENTATION](../RPI%20IMPLEMENTATION/README.md), not the preserved original `Arduino/ConnectFourGameTest`.

Read [handoff 007](007_THREE_BUTTON_MENU_CONTROLS.md) for the current three-button mappings and firmware verification, [handoff 005](005_CURRENT_STATE_AND_NEXT_CHECKS.md) for the physical limitations, and the [USB protocol](../RPI%20IMPLEMENTATION/PROTOCOL.md) before changing either end. The two-button mappings in handoff 006 are superseded.

## Physical setup at the last reported check

- Raspberry Pi 4B running OS Lite 64-bit, connected to the laptop's network and powered through USB-C from the laptop.
- ESP32 connected to a Pi USB-A port through its USB serial adapter.
- ESP32 was on its own: PCA9685, OLED, buttons, sensors and servos were not connected during these checks. Hardware wiring remains the next main task.
- Speaker identified as **JBL Go 4**, Bluetooth address **10:28:74:C4:25:88**.
- Pi shell showed `matthew@matthew`; OS reported **Debian GNU/Linux 13 (trixie), 13.7**. WirePlumber reported **0.5.8**.
- Laptop USB power was sufficient for the reported setup tests, but supply capacity and operation with additional connected hardware have not been qualified.

## What worked

| Check | Result and evidence |
| --- | --- |
| SSH and deployed Python service | Operator reached the Pi shell and ran the service from `~/stormhacks-rpi/pi` using `.venv/bin/python`. |
| USB device detection | `/dev/serial/by-id/` showed the Silicon Labs CP2102 adapter linked to `ttyUSB0`. |
| Pi-to-ESP32 serial communication | The service printed `USB connected`, a valid empty board snapshot, `free / Fault / move 0`, and ESP32 messages. |
| Terminal command forwarding | Entering `help` in the running Pi service returned the ESP32 command list. |
| Bluetooth radio | After the unblock/service-restart steps, `power on` succeeded and `show` reported `Powered: yes`. |
| JBL pairing | Device information showed `Paired: yes`, `Bonded: yes`, `Trusted: yes`, `Blocked: no`, and Audio Sink/A2DP UUIDs. |
| Audio services | PipeWire, pipewire-pulse and WirePlumber were all active under the Pi user's systemd session. |
| Bluetooth audio playback | Following the headless WirePlumber configuration below, the operator reported hearing the ordinary test audio through the speaker. |
| Standalone Gemini and ElevenLabs checks | Operator confirmed the separate API-test procedure worked. The procedure requested a Gemini hint for a sample empty board, generated an ElevenLabs WAV, and played the speech. Exact API response text was not supplied. |

The standalone API tests used a sample board without commanding ESP32. They establish provider access and audio playback independently of the disconnected game hardware. They do not establish live move-triggered coaching, post-game reviews or physical gameplay.

## What failed, and how it was handled

### Expected PCA fault with disconnected hardware

Both `--local-only` and `--no-speech` reached ESP32 but reported:

```text
ERROR: PCA fault. Outputs disabled; repair and reset ESP32.
```

This is expected while PCA9685 is absent. Those flags control Pi API/audio behavior; they do not bypass ESP32 hardware checks. `help` still works in fault state, but clearing confirmation and game setup cannot clear the latched PCA fault. Connect the PCA logic/OE correctly, then reset ESP32. Do not disable the fault handling just to make a disconnected board enter gameplay.

### Bluetooth radio would not power on initially

Initial errors were `org.bluez.Error.Failed` from `power on` and `org.bluez.Error.NotReady` from discovery. One earlier attempt also reported `bluetoothctl: command not found`; subsequent attempts had the command available.

The radio subsequently powered on after the suggested steps:

```bash
sudo rfkill unblock bluetooth
sudo systemctl restart bluetooth
```

The original cause was not independently isolated. `Discoverable: no` on the Pi was not a problem for connecting outward to the speaker.

### Speaker pairing succeeded, but audio connection failed

Repeated connection attempts returned:

```text
org.bluez.Error.Failed br-connection-profile-unavailable
```

Trusting the device and repeating `connect` did not resolve it. Later attempts produced no success message, and `info` still showed `Connected: no`, despite the correct JBL name, pairing and audio UUIDs.

The audio stack installation/startup procedure included PipeWire, pipewire-pulse, pipewire-audio, pipewire-alsa, WirePlumber, libspa-0.2-bluetooth, pulseaudio-utils, BlueZ and alsa-utils. Service status confirmed all three audio services were running. Their running state alone did not establish that Bluetooth profiles were available to the SSH user.

### Headless WirePlumber configuration resolved playback

WirePlumber's Bluetooth seat monitoring can reserve audio devices for an active local session, excluding a headless SSH session. The instructed user configuration was:

```text
~/.config/wireplumber/wireplumber.conf.d/80-headless-bluetooth.conf
```

```text
wireplumber.profiles = {
  main = {
    monitor.bluez.seat-monitoring = disabled
  }
}
```

Then, as the normal Pi user, without sudo:

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
```

The operator subsequently confirmed hearing the speaker audio. This is the configuration to preserve for the SSH-operated Pi. See [WirePlumber's headless Bluetooth guidance](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/bluetooth.html).

Select the actual `bluez_output...` sink using `pactl list short sinks`, set it as default, unmute it, and use `AUDIO_DEVICE=default` for the application's ALSA playback. The exact selected sink name was not recorded. Successful playback does not yet establish automatic reconnection after a Pi/speaker reboot or SSH logout.

### Initial journal command returned no entries

`journalctl --user -u wireplumber` returned `No journal files were found`, although systemd status displayed truncated messages. Suggested alternatives were full status with `--no-pager -l` and:

```bash
sudo journalctl -b _SYSTEMD_USER_UNIT=wireplumber.service --no-pager -n 60
sudo journalctl -u bluetooth -b --no-pager -n 40
```

No later diagnostic output was needed once playback worked.

## Configuration and normal startup

Pi project directory:

```text
~/stormhacks-rpi/pi
```

The confirmed serial-device setting is:

```dotenv
SERIAL_PORT=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

Keep API keys in the Pi's `.env`; never copy their values into a handoff, screenshot, commit or log. Preserve the working model/voice settings on the Pi. The deployed configuration initially showed `gemini-3.5-flash-lite`, ElevenLabs voice `JBFqnCBsd6RMkjVDRZzb`, model `eleven_flash_v2_5`, `SPEECH_ENABLED=true`, and `AUDIO_DEVICE=default`. The current remote `.env` was not inspected; do not overwrite it with the laptop's blank template when redeploying code.

After SSH connects, run these as separate shell commands:

```bash
cd ~/stormhacks-rpi/pi
.venv/bin/python -m connect4
```

ESP32 starts its own firmware when powered. The Pi program opens the configured serial port and receives snapshots; it does not upload firmware or automatically start a game. Compatible integration telemetry was observed, but the precise uploaded firmware revision and its physical three-button behavior were not verified.

Once hardware is connected and ESP32 is out of fault state, physically clear the board, indexer and feed path, then type this **inside the running Pi Connect Four terminal**, not in the Bash shell or Arduino IDE:

```text
confirm-clear
```

Press Enter. The Pi forwards the command over USB. Then use Left/Right to browse and Centre to confirm. Keep Arduino Serial Monitor and other serial owners closed. Human columns are still entered through this terminal after placement; human IR sensing remains disabled.

`stop` disables ESP32 outputs. `quit` exits the Pi program without issuing `stop`; when stopping a hardware test, enter `stop` first, then `quit`. Faults require their documented repair/reset sequence. USB reconnects never authorize replaying an uncertain physical move.

## Architecture and verification boundaries

ESP32 owns game state, minimax, buttons, OLED, sensors, motors, fault handling and move commitment. Pi owns API calls, coaching explanations, logs and speech. Network/API failures must not bypass or block ESP32 safety logic. Robot moves still require the qualified target IR passage, quiet time and completed hatch closure.

Previously completed software verification from handoff 007 remains applicable: four native suites and 19 Python tests passed; ESP32 compile used 338360 bytes flash and 33876 bytes global RAM. No new compile or hardware test was performed when writing this handoff.

The OLED was not connected or visually tested during Pi setup. Its menu text passed prior static bounds checks, but actual appearance is pending. Buttons, real IR timing, servo position, magazine isolation, hatch retention, integrated speech during play, completed-game review and loaded gameplay remain unverified. No successful physical move or dispensing cycle was reported during these Pi-only checks.

## Next work, in order

1. With USB and servo power disconnected, wire OLED, three buttons, PCA logic/OE and all seven IR modules using the [current wiring guide](../RPI%20IMPLEMENTATION/firmware/ConnectFourGameTest/README.md). Left is GPIO13, Right GPIO14, Centre GPIO23. Preserve the external 10 kOhm OE pull-up and common ground; servo V+ must have its own suitable supply path.
2. Keep servo power off. Power the logic, reset ESP32 and check the PCA fault clears. Verify the actual OLED menus and three-button press/release behavior. If the uploaded build is uncertain, manually upload the current integration sketch first.
3. Validate each IR module's DO voltage, polarity and passage timing. GPIO34–39 require external/module pull-ups. Clear sensors do not prove connectivity or final disc seating.
4. Calibrate enabled servos unloaded. Keep PCA channels 2–4 disabled/disconnected until their reported full-revolution behavior is identified and corrected; those are hatch columns 3–5. PCA7 is the indexer. Never route servo current through ESP32 or breadboard logic rails.
5. Test unloaded human/robot sequencing, sensor-confirmed commitment, wrong/extra/missing passages, stop/restart and supervised recovery. Confirm no automatic second release on failure.
6. Run the Pi service with real game events and test live Coach hints/feedback, Free Play end review, Gemini-to-ElevenLabs speech, logs, stale speech cancellation and network/audio loss. Standalone provider tests do not replace this check.
7. Check speaker/Pi reboot and SSH reconnect behavior, including restoration of the Bluetooth default sink. Assess Pi supply capacity before expanding the connected hardware.
8. Only after mechanical retention, calibrated travel and one-disc isolation are established, proceed to a controlled loaded prototype test.
