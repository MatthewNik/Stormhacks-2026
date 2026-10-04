# Handoff 018: PCA fix works; IR capture overflow remains

## Current status

The full offline Connect Four game is installed on the Pi and ESP32. The PCA control method from the working standalone motor program fixed startup: the game now initializes the servos, reaches the menus, accepts the Left/Right/Centre controls, and starts a human turn. Coach remains visible but unavailable. Pi has no active Gemini, ElevenLabs, speech, or coaching service calls.

During human play, the Pi prompts:

```text
Insert ONE human disc; ESP32 IR sensors register its column automatically.
Sensor backlog or overflow
Delivery paused; board/pending move frozen. Use correct or restart; no automatic feed retry.
```

The human move is not committed. This is the intended fail-safe response when sensor capture is ambiguous, but ordinary insertion should not overflow the queue. Investigate and fix the cause before loaded play.

## What worked

- PCA initialization now matches the working standalone motor program: `Wire.begin(SDA,SCL)`, 50 ms timeout, Adafruit driver setup, oscillator/frequency configuration, and prescaler-aware 500–2500 us servo pulses.
- Register reads first use the motor program's repeated-start sequence; a failed combined read gets one checked STOP-separated retry. Failed/short reads still fault; there is no guessed value or skipped health check.
- PCA initializes before OLED and sensors. The Pi package reports if USB is open but no ESP32 game snapshot arrives.
- Servo angles, staggered startup, continuous PWM holding, OLED setup screens, menu buttons, offline game, and IR-only human move selection are integrated.
- Full ESP32 build succeeded. Five firmware native suites passed. All 24 Pi tests passed. Merged firmware and Pi ZIP checksums/package contents verified.

## What did not work / remains unverified

- The human IR sensor path currently pauses with `Sensor backlog or overflow` instead of registering the inserted disc.
- We have not identified whether the queue is filling from ISR chatter/contact bounce, noisy or floating sensor inputs, polarity/beam alignment, an interrupt queue/counter bug, or delayed polling while the engine/radio/display code runs. The status text identifies the protection that fired; it does not by itself prove which cause occurred.
- We have not verified physical sensor polarity, idle levels, passage waveforms, or queue depth on the actual board. Do not suppress the overflow fault or commit a move when events are ambiguous.
- Do not assume the PCA change caused the sensor overflow; the two subsystems have not been shown to share a failure.

## Next investigation

1. Read `working code/firmware/ConnectFourGameTest/Esp32Sensors.h`, `Sensors.h`, `Controller.h`, and `GameConfig.h`, plus `tests/sensor_capture_tests.cpp` and the native sensor tests.
2. Trace every path that reports `Sensor backlog or overflow`; distinguish queue overflow from backlog/invalid-edge handling. Confirm the ISR captures only while a human/manual capture window is armed and that reset/seal atomically clears the queue and epoch.
3. Instrument diagnostics without weakening safety: report per-column initial levels, edge counts, queue high-water mark/overflow flag, edge timestamps, and sensor levels at pause. Bound serial output and avoid doing slow I/O in the ISR.
4. Reproduce insertion with one sensor at a time unloaded. Check active-low polarity, pull-up/electrical output requirements, wiring, beam alignment, and whether sensor chatter creates edges faster than the main loop drains them. Record readings at idle, beam blocked, and a slow single passage.
5. Add/update deterministic tests for bounce, queue capacity, sensor chatter, overlapping passages, capture reset/seal races, and delay under robot-search polling. Keep ambiguous captures paused without move commitment or feeder retry.
6. Rebuild the full-hardware firmware and Pi package only after tests pass. Keep all outputs in the active workspace. Verify on hardware with the feed path empty before loaded play.

## Relevant implementation details

- ESP32 owns game state, IR capture, menu buttons, OLED, PCA, fault handling, move commitment, robot search and delivery. Pi is terminal/history only.
- Sensor GPIO order is columns 1–7: `{34,35,36,39,32,33,27}`; all are configured active-low. ISR queue capacity is 64 events. Qualification constants are in `GameConfig.h` (`DETECT_US`, `CLEAR_US`, `STABLE_CLEAR_US`, `STUCK_US`, baseline and passage timeouts).
- Human moves must come from one qualified IR passage in a legal column; typed columns and serial `move` requests are rejected as sensor-only.
- The repo includes a separate `motor command` project with the PCA approach that worked. Preserve its unrelated edits and do not replace the full game with the standalone diagnostic firmware.
- Existing deployment artifacts in `working code/deployment/` correspond to the PCA update (build 017). If code changes, rebuild and verify new artifacts; do not mistake existing binaries for changes made by the next agent.

## User's current symptom

The Pi prints the human sensor prompt, then `Sensor backlog or overflow`, then pauses with the pending board/move frozen. The user can assign another agent to investigate this IR sensor issue.
