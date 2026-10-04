# PCA register-read transaction update

## Observed failure

The operator supplied `PCA detail: register 0xFE read failed; address=0x40 SDA=21 SCL=22 code=-1 observed=0`. Firmware had passed the address probe and Adafruit initialization, then received zero bytes from its custom PRE_SCALE read. The current ESP32 core is 3.3.12. Its `endTransmission(false)` defers the pointer write until a combined write/read request; a zero return there does not verify that pointer selection reached the chip.

## Changes

The full-game `PcaHardware` now completes pointer selection with `endTransmission(true)`, verifies the actual write result, then requests one byte with an explicit STOP. Bus initialization specifies 100 kHz on GPIO21/22. The returned byte is checked before conversion. Short/missing reads still fault, and startup frequency, awake-mode, runtime prescaler and all output-write checks remain required. No register values are guessed, fault checks bypassed or automatic disc retries added. This targets the reported combined-read failure; confirmation on the physical ESP32/PCA remains pending.

Pi suppresses identical repeated PCA detail text after its first display. Reconnect, a changed boot ID or `diagnose` permits another display; changed diagnostic text always displays. Firmware no longer repeats the unrelated OLED explanation line. Fault snapshots and game logs remain available. OLED still shows setup menus and plain status without the board grid. All gameplay, calibration, holding, IR and unavailable Coach behavior remain as in handoff 014.

## Validation and package status

The Wire stub models deferred combined transactions and a device/bus that returns zero bytes for repeated-start reads while responding to address probes. The new regression failed with the previous production code, then passed with STOP-separated transactions. Tests also verify real pointer-selection failure, missing read bytes, retained hardware health checks and explicit 100 kHz configuration. This is a controlled software reproduction of the symptom, not a measurement of the physical bus.

All five native firmware suites and all 22 Pi tests passed. Full ESP32 compilation passed: **340008 bytes flash / 33980 bytes RAM**. Deployment firmware, Pi ZIP and checksums were regenerated and verified against the final build/current source. ZIP integrity and both SHA256 entries passed. The firmware includes the additional missing-byte check. No credentials, environments or logs are packaged.

Use [DEPLOY.md](../working%20code/DEPLOY.md) to transfer all three current artifacts, extract Pi source and flash ESP32 with servo power off and the terminal closed. Existing Pi dependencies are unchanged. Start the terminal with the same CP2102 serial path. Successful startup should position doors/indexer and reach Mode. A continuing fault must retain the new `PCA detail:` output for further diagnosis. No remote flashing, physical movement or electrical measurement was performed.

## Implementation references

- [Espressif Wire implementation](https://github.com/espressif/arduino-esp32/blob/master/libraries/Wire/src/Wire.cpp): deferred non-STOP transmission and combined read behavior; the installed 3.3.12 source was inspected locally.
- [NXP PCA9685 data sheet](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf): register selection and I2C operation.
