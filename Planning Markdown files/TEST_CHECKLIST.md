# Integration and demonstration test checklist

Planning only: boxes below are not claims of completed hardware tests. Use PROJECT_PLAN.md and WIRING_AND_CALIBRATION.md as the source of expected behavior.

## Evidence to record

For every stage, record date, operator, board/part revision, firmware version, ESP32 core/library versions, calibration settings, result and raw log path. Log uptime, controller state, raw sensor transitions, qualified passages, pending move identifier, expected/actual column, servo phase, board heights and AI duration/node count. Use user-facing columns 1-7 in operator messages and label internal 0-6 values explicitly.

## Stage 1: existing engine on the ESP32

- [ ] Upload the existing Serial Monitor sketch without actuator power.
- [ ] Play human-first and robot-first games at easy, medium and hard.
- [ ] Record search duration and free heap in opening, middle and late positions.
- [ ] Check invalid/full-column input does not alter the board.
- [ ] Check both colours' horizontal, vertical and diagonal wins and a draw using native tests or controlled positions.
- [ ] Confirm AI simulations leave the authoritative board unchanged until the selected move is committed.

Pass: legal gameplay, correct turn/result handling and usable measured search times. Prior successful compilation alone does not satisfy this stage.

## Stage 2: logic, OLED and buttons

- [ ] Check logic rail, DO and I2C idle voltages are 3.3 V-safe.
- [ ] Confirm OLED controller is SH1106 and record its I2C address.
- [ ] Confirm PCA address and OE disabled boot state, without servo power.
- [ ] Test difficulty/first-player cycling, one action per press and hold-to-request-restart.
- [ ] Confirm an empty-board confirmation is required before starting/restarting.
- [ ] Read the small stored-board view at normal operator distance; distinguish filled red from outlined yellow.
- [ ] Test missing OLED/PCA response produces a clear initialization failure rather than starting motion.

## Stage 3: sensor qualification

- [ ] Mount one sensor in the real chute; tune using both colours.
- [ ] Log at least 100 red and 100 yellow drops with no missed or duplicate qualified passages.
- [ ] Include maximum gap/wobble, slow/fast drops, centre-hole alignment trials and expected venue lighting.
- [ ] Run at least 10 minutes empty with no false qualified passages.
- [ ] Verify held disc becomes stuck-sensor fault; removal rearms only after a valid clear interval.
- [ ] Characterize an unplugged sensor: document whether it is distinguishable from clear.
- [ ] Repeat at least 50 drops of each colour on every final column.
- [ ] Activate all seven sensors and verify neighbouring-column isolation.
- [ ] Drop while hard-mode search runs; compare raw and qualified events to detect loss.
- [ ] Inject queue overflow; require pause without an invented board update.

Pass: zero observed misses, duplicates and idle false detections in the stated trials. These sample counts are a minimum hackathon acceptance baseline, not a statistical guarantee. If any trial fails, adjust hardware/filtering and repeat the affected qualification.

## Stage 4: hatches and magazine

- [ ] Verify each hatch channel maps to its labelled physical column.
- [ ] Calibrate unloaded endpoints before adding discs.
- [ ] Verify staggered human opening and robot closing produce no reset/sensor glitches.
- [ ] Check full columns stay closed during the human turn.
- [ ] Measure startup/holding loads with the power teammate and confirm acceptable rails/wiring.
- [ ] Verify stack -> upstream stop -> one-disc pocket -> downstream stop layout.
- [ ] Execute at least 30 releases at low, 30 at medium and 30 at full magazine fill; count physically.
- [ ] Require zero double-feed and zero uncommanded feed; any failure stops integration.
- [ ] Test power/PWM removal and operator cutoff with a secured mechanism; record actual retention behavior.
- [ ] Verify manual removal access for a trapped disc without commanding a second release.

## Stage 5: complete delivery and games

- [ ] Deliver at least 10 robot discs to each column from the shared rail.
- [ ] Observe the target sensor before, during and after servo phases; accept an early passage only once.
- [ ] Confirm robot move stays pending until passage and physical cycle conditions complete.
- [ ] Check human discs occupy the lowest empty row; colour follows turn, not sensor reading.
- [ ] Confirm Thinking appears before search and Your turn only after hatches are ready.
- [ ] Run at least one complete game for each difficulty and first-player combination.
- [ ] Verify winning human/robot moves end play without an additional robot release.
- [ ] Empty manually and start another game; matrix resets only after explicit confirmation.

## Stage 6: fault and recovery trials

| Injected event | Required result | Recovery acceptance |
| --- | --- | --- |
| Extra human disc before robot completion | Pause; no automatic next feed | Remove extra, compare OLED board, confirm correction |
| Robot disc never reaches target | Pending move remains uncommitted; timeout | Account for original disc, arm manual completion, one target passage commits once |
| Robot disc enters wrong column | Pause; wrong disc not committed | Remove misplaced disc, arm intended column, manually deliver through target sensor |
| Second disc during manual completion | Pause again | No double commit; correct or restart |
| Two columns trigger ambiguously | Pause; no guessed order | Account for all discs or empty/restart |
| Full-column detection | Pause | Remove unexpected disc and verify board |
| Sensor held active or disconnected | Fault if detectable; no false self-test claim | Repair and retest; unplugged-as-clear limitation documented |
| PCA/I2C failure | No further automatic feed; board frozen | Secure mechanism, repair, reinitialize explicitly |
| ESP32 reset during a release | No automatic release or game resume | Secure feed path, empty board, restart |
| Restart requested with discs still present | Confirmation screen, no immediate matrix clear | Operator clears board/feed path, then fresh confirmation |

Test phase-aware stop behavior with a secured mechanism. Do not assume disabling PWM closes gates or that sensor-clear proves no disc is trapped elsewhere. Correction-mode events are logged but cannot commit a move; manual completion must be armed explicitly.

## Demo readiness

- [ ] All stage records completed with unresolved failures listed as blockers.
- [ ] Calibrated configuration matches labels, current mechanism and document revision.
- [ ] Battery/power interface signed off by teammate; cutoff accessible.
- [ ] Magazine refilled, board/rail clear, hatch and stop positions checked.
- [ ] Operator can explain both manual recovery and full restart without a computer.
- [ ] No automatic feed retry in any timeout branch.
- [ ] Guide PDF, editable draw.io, calibration sheet and raw logs available offline.

Fallback: if a physical subsystem remains unreliable, demonstrate the existing serial game and clearly identify the mechanical integration as incomplete. Do not present a software-only demonstration as a qualified physical robot.
