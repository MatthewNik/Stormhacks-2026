# Current state and next checks

## Workspace and entry point

Continue only in `C:\Users\matth\Documents\Stormhacks2026 Oct 3-4`; read its AGENTS.md before editing. The older Stormhacks2026 directory remains reference-only. Repository: `MatthewNik/Stormhacks-2026`, branch `main`.

Current firmware is [ConnectFourGameTest](../Arduino/ConnectFourGameTest/ConnectFourGameTest.ino). Read its [README](../Arduino/ConnectFourGameTest/README.md) for commands, wiring, timing defaults, tests and bench limitations. Earlier standalone servo, sensor and display tests are preserved.

## Implemented behavior

- ESP32 owns the board, AI, actuators, sensing, OLED and commitment. No Pi/network dependency.
- Serial Monitor at 115200 baud handles human moves and commands. OLED uses the confirmed 96x64 SSD1331 SPI wiring: clock 18, data 19, reset 16, DC 17, CS 26.
- Startup requires `confirm-clear`, then `0`/`1` and `easy`/`medium`/`hard`. The starting player retains O, the other X. Minimax uses 2/4/5 plies with alpha-beta and board copies; hard is not guaranteed perfect.
- Human moves are typed columns 1-7. IR capture is disabled throughout human phases, menus, game end and unarmed correction.
- Robot delivery closes hatches, saves a pending AI column, opens that target, checks stable-clear sensors, loads PCA7 at 90 degrees, settles 300 ms and waits 500 ms, then releases at 0 degrees and settles 300 ms.
- The robot board update requires exactly one expected IR passage after release, quiet time and completed hatch closure. No timer alone commits a move.
- One scheduler owns PCA0-7 and independent pulse expiry. Channels 8-15 stay FULL_OFF. Ordinary commands expire after 300 ms; selected robot hatch commands after 1600 ms.
- Wrong/extra/premature/missing/stuck/noisy/overflowed sensor history pauses without another indexer release. Board and pending move are retained.
- `correct`, `arm-manual`, and `confirm-correction` support supervised pending-move completion without commanding PCA7. `restart` requires `confirm-clear` before clearing stored state. PCA faults require repair and ESP32 reset.

## Isolated motors and bench boundary

The operator reported repeated full revolutions on **printed PCA connectors 2-4**, believed to be 180-degree servos; exact models and physical repair are unconfirmed. Those channels remain disabled. They correspond to game columns 3-5. The operator explicitly permits software selections/IR confirmation in those columns during this **unassembled bench test**; their hatch outputs never receive position pulses. OLED marks an isolated pending target with `!`.

Pulse expiry removes holding torque. The 3000 ms IR timeout can outlast the 1600 ms hatch drive window. This firmware does not guarantee loaded-hatch retention, one-disc magazine isolation or safe stack holding. The software pulse limits are not an independent power cutoff if ESP32 hangs. Do not treat a successful command or sensor passage as proof of final seating or servo position.

The reference PDF/draw.io flow was consulted read-only for commitment/recovery concepts. Current requirements replace its human sensing and two-stop magazine arrangement with typed human moves and one PCA7 indexer.

## Verification completed

- All **three native suites passed**: game/controller/AI/output sequencing, PCA adapter, and production ESP32 capture adapter under GPIO/interrupt stubs.
- Coverage includes both starters, symbols, gravity, invalid/full columns, all win directions, draws, AI wins/blocks and immutable boards; ignored/stale human events, noise, early/wrong/extra/missing passages, stuck inputs, overflow/backlog and timer wraparound; PCA7 ordering/expiry/shared OE; stop/restart/fault interruption, manual recovery, clearing confirmation and exactly-once commitment.
- Independent final code review found timing underflow, unbounded noisy waiting and unbounded queue-drain risks. They were fixed and regression-tested; final review found no remaining actionable defects.
- Final **ESP32 Dev Module compile passed**: 324256 bytes flash, 25228 bytes global RAM. ESP32 core 3.3.12; PWM Servo Driver 3.0.3; GFX 1.12.6; BusIO 1.17.4; bundled SSD1331 driver 1.3.0 with upstream license.
- Final build is under `tmp/connect-four-game-test/esp32-ir-build`. An initial toolchain archive error was resolved with a fresh build folder and one job. Temporary build/test files are excluded from Git; the small Arduino CLI configuration is retained for the documented compile command.
- **No upload performed.** Software checks do not establish actual interrupt latency, multicore contention, electrical behavior, sensor thresholds, servo travel or physical disc delivery.

## Next operator checks

1. Follow the README's unloaded checklist; manually upload only when ready. Verify clearing/menu/gameplay layouts on OLED.
2. Verify all seven IR modules are connected, active-low as configured, and 3.3 V-safe. GPIO34-39 require suitable external/module pull-ups. Confirm clear baselines and deliberate passage timing.
3. Identify and test the spinning servos unloaded before re-enabling PCA2-4. Validate PCA7 positional behavior, indexer endpoints and load/release timing without magazine discs first.
4. Check human IR activity is ignored, correct robot passage commits once, and wrong/extra/missing passages pause without feeding again. Exercise supervised correction and clearing confirmation.
5. Establish mechanical retention, one-disc isolation and calibrated travel before any loaded prototype run. Preserve sensor-confirmed commitment and supervised recovery.
