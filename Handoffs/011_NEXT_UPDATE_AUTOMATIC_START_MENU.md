# Full version status and next-update startup request

## Current version

Continue in the active Oct 3–4 workspace. The full hardware source and deployment artifacts are in [working code](../working%20code/README.md). Follow [current wiring](../working%20code/WIRING.md) and [deployment commands](../working%20code/DEPLOY.md). The preserved `button test` directory is a separate menu-only version; do not deploy it for full hardware operation. `RPI IMPLEMENTATION` remains preserved.

Handoff [010](010_FULL_HARDWARE_WIRING_AND_UPLOAD.md) records the latest wiring and verification. Full firmware enables PCA9685, all seven calibrated hatches, the PCA7 magazine indexer, seven IR inputs, SSD1331 SPI OLED, and three buttons. ESP32 retains ownership of game state, motor sequencing, IR qualification, faults and move commitment; Pi provides terminal, coaching and speech.

Columns are numbered right to left from the front: column 1/PCA0/IR GPIO34 through column 7/PCA6/IR GPIO27, with intermediate IR pins 35,36,39,32,33. Human moves remain terminal-entered; robot moves require IR confirmation. Flap command starts are at least 50 ms apart. PCA7 loads at 80 degrees and releases at 145 degrees outside flap batches. Left GPIO13, Right GPIO14, Centre GPIO23 operate menus.

The latest full build passed all five native firmware suites and all 19 Pi tests. ESP32 compilation used 338356 bytes flash and 33876 bytes RAM. Full-mode build metadata, deployment checksums and Pi archive contents were verified. No actual full-version upload or loaded hardware validation has been reported in this conversation. Existing artifacts still require `confirm-clear` before initial menu navigation.

## Explicit user request for the next device update

The user requests that the next RPi/ESP32 update no longer require typing `confirm-clear` to start. After successful ESP32 hardware initialization, enter the normal Mode menu automatically so the three buttons can be used immediately. This request is authorized for implementation on the next update; it has not been implemented by this handoff-only change.

Implement initial startup in the full firmware, not by having the Pi automatically send a clearing or gameplay command. Initialize the new empty logical game, game identifier/history and default Free Play menu consistently with the current clearing path. Opening the menu must leave motor outputs disabled and sensor capture disarmed until the existing gameplay sequence requires them. Centre still confirms mode, difficulty and starter before gameplay; startup does not automatically dispense a disc or select a game.

Preserve normal PCA initialization checks and reset-only hardware faults. Preserve explicit clearing/recovery behavior after a stopped, paused or uncertain physical move; the request removes the initial startup text command, not supervised recovery or uncertain-move handling. Do not introduce automatic retries or replay over USB reconnect. Physical board/indexer/feed-path clearing remains an operator prerequisite before beginning a new game; sensors cannot establish an empty physical board.

No unattended Pi service or automatic SSH-login launch was requested. Preserve the current foreground Pi startup command unless the user separately asks to change it.

## Next implementation and deployment checks

- Add startup coverage proving healthy full hardware enters Mode directly without `confirm-clear`, with correct defaults and a coherent Pi snapshot/history.
- Verify missing/failing PCA still enters Fault with outputs disabled; initial menu entry produces no servo commands or disc release.
- Retain button navigation/release behavior, calibrated angles, 50 ms flap staggering, IR-column mapping, robot confirmation and recovery tests.
- Update Pi-facing startup instructions/messages as needed, rebuild full firmware, run firmware/Pi tests, regenerate deployment packages/checksums, and provide full-version PowerShell/SSH commands without an initial `confirm-clear` step.
- Keep the current saved button-test version and original integration directory unchanged. Put all changed source and build outputs inside the active workspace's `working code` directory.
