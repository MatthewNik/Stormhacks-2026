# Robot-only IR and PCA7 indexer

Current integrated version: [ConnectFourGameTest](../Arduino/ConnectFourGameTest/README.md).

Human moves stay typed. Sensor capture is disabled on the human turn and in menus/unarmed correction. Robot delivery uses timestamped IR edges on GPIO34,35,36,39,32,33,27, requiring exactly one expected-column passage. The robot move remains pending until release settling, quiet time and hatch closure complete; the old automatic one-second commitment is removed.

PCA7 is now the single indexer: load90, settle300ms plus fill500ms, release0, settle300ms. One scheduler owns outputs0-7 and their bounded pulse deadlines. Channels8-15 remain off. PCA2-4 remain isolated; the operator explicitly permits software selection/IR confirmation of those columns during unassembled bench tests. Selected hatch pulses remain bounded1600ms and ordinary pulses300ms, so this is not qualified loaded-mechanism control.

New commands: confirm-clear at startup/restart, correct, arm-manual and confirm-correction for supervised pending-move completion without another indexer release. Wrong/extra/premature/missing/stuck/overflow/backlogged sensor events pause with board and pending move frozen. PCA faults remain reset-only.

All three native suites passed, including production ISR-adapter gating/queue behavior under stubs. Independent code review identified quiet-time underflow, unbounded noisy waiting and unbounded queue-drain risks; these were corrected and regression-tested. No further actionable defects were found in the final review. Tests do not verify real ISR latency, multicore contention or physical behavior.

Final ESP32 Dev Module compilation passed: **324256 bytes flash**, **25228 bytes global RAM**, using ESP32 core3.3.12 and the documented Adafruit versions. Outputs are in `tmp/connect-four-game-test/esp32-ir-build`. The first attempt hit a toolchain archive-file error; a fresh workspace build folder with one job resolved it, and the final source was compiled successfully. No upload performed.

Use the README's bench checks next. Sensors require module polarity/pull-up qualification, indexer endpoints/loading duration require calibration, and released servo pulses provide no guaranteed holding torque or stack retention. Reference PDF/draw.io were consulted read-only for commitment/recovery logic; current user instructions replace their human sensing and two-stop magazine assumptions.
