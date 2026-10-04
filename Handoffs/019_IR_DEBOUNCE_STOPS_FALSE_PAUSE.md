# Handoff 019: one disc no longer pauses as a sensor backlog

## What was happening

Free Play reached HumanReady. Inserting one disc lit that column's IR module LED, then the Pi printed `Sensor backlog or overflow` and the OLED stayed on pause. The move was not committed, so the robot never answered.

The capture interrupt queued every CHANGE edge. A disc passage chatters the comparator much faster than the main loop drains the 64-edge queue, so `poll` paused before the 2 ms / 20 ms qualification could accept the passage. Idle baseline could still pass, because the pins are quiet until a disc arrives.

## What changed

`Esp32Sensors` now records a column level only after it has remained unchanged for `SENSOR_DEBOUNCE_US` (2 ms, the same minimum as detection). Shorter edges are dropped and do not fill the queue. A stable block still becomes one active edge and, after the beam clears, one inactive edge. Qualification, sealing, wrong-column, extra-disc, stuck, and real queue-overflow pauses are unchanged. There is still no automatic feed retry.

Boot text is Connect Four build 018. Native firmware suites cover chatter that must not overflow, a noisy passage that qualifies once, a pulse that finishes before the next poll, two simultaneous columns, and a real 64-edge overflow.

## Install

Rebuild with `working code/compile-firmware.ps1` and `working code/package-deployment.ps1`. Transfer `firmware.bin`, `pi-update.zip`, and `SHA256SUMS` as in `working code/DEPLOY.md`. Servo power off, terminal closed, flash the merged image at `0x0`, then reset ESP32. The terminal must show build 018.

Clear the board and feed path. Select Free Play, Easy, human first. At Insert ONE disc, drop one disc through one column. The terminal should leave HumanReady, show that column occupied, and start the robot turn. Do not type the column. If the module LED stays on after the disc has passed, the beam is still blocked and the turn correctly waits or pauses as stuck; that is a sensor alignment issue, not another backlog.
