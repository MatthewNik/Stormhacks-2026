# PCA fault diagnostics and simplified OLED

## Reported issue

The operator reports an immediate PCA fault when starting the Pi terminal through the CP2102 serial path. The reported log contains only the previous generic PCA message. The cause is still unconfirmed. Pi startup opens serial; it does not initialize PCA directly. Serial opening may expose an existing ESP32 fault or cause a reset-time fault. The light on PCA alone does not identify the failed firmware check.

## Current changes

Deploy the updated [working code](../working%20code/README.md) packages. OLED retains Mode (Free Play / unavailable Coach), Difficulty (Easy / Medium / Hard) and Who starts? (Human / Robot) menus. During play it shows plain turn/status text and a pending robot column, without a game board or disc graphics. The Pi terminal still prints the board.

PCA firmware now preserves its first failure and reports `PCA detail:` with the failed step, configured address 0x40, SDA21/SCL22, numeric error code and observed register/channel value. Steps distinguish I2C/address probe, Adafruit initialization, register selection/read, frequency validation, changed prescaler, sleep mode, FULL_OFF writes and motor-position writes. Shutdown writes cannot replace the original cause.

`diagnose` in the updated Pi terminal requests the cached report and snapshot. Fault snapshots automatically repeat the report so reconnecting after a failed boot does not lose its cause. The command does not access/reconfigure hardware, move motors, clear faults or retry feeding. The added startup address probe checks the configured address before driver initialization; all existing fault/health checks remain enabled. No hardware or software root-cause fix is claimed without the new diagnostic output.

## Validation and deployment

All five native firmware suites and all 21 Pi tests passed. Added checks cover diagnostic failure categories, first-error retention through shutdown, runtime prescaler/sleep/position-write errors, serial diagnostic dispatch and fault-snapshot reports, and Pi forwarding. Existing game, buttons, holding, IR, correction and complete-game regressions still pass.

Final ESP32 compilation passed: **339964 bytes flash / 33980 bytes RAM**. Firmware, Pi ZIP and SHA256 manifest were regenerated. Firmware matches the final full-hardware merged build and contains diagnostic markers; ZIP integrity and every packaged Pi source match passed. Credentials, environments and logs are excluded.

Use [DEPLOY.md](../working%20code/DEPLOY.md) to transfer all three artifacts, extract Pi code and flash ESP32 with servo power off and the terminal closed. Existing Pi dependencies are unchanged. Run the same terminal startup command, then retain the `PCA detail:` line (or enter `diagnose`). That hardware-side observation is the next step needed to identify why the fault occurs. No remote flashing or physical diagnosis was performed during this update.
