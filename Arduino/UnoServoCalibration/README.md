# Uno single-servo calibration

Open `UnoServoCalibration.ino` in Arduino IDE. Select **Arduino Uno**, the Elegoo Uno R3's USB port, and upload. Install the **Servo** library by Arduino if it is missing. Open Serial Monitor at **115200 baud**, with **Newline** or **Both NL & CR** selected. No PCA9685 is needed.

## Wiring

Disconnect USB and servo power while wiring. Use one unloaded servo at a time.

| Connection | Destination |
| --- | --- |
| Servo orange/yellow signal wire | Uno digital D9 |
| Servo red positive wire | External regulated 5 V supply positive |
| Servo brown/black ground wire | External supply negative |
| Uno GND | External supply negative (common ground) |
| Uno USB | Computer, for Uno power and Serial Monitor |

Use a regulated 5 V supply with at least 1 A capacity for this single-servo bench test; verify the actual servo's rating. Run servo power and return directly through suitable wiring. Keep the external positive supply separate from Uno 5V/VIN and USB. Do not use a 9 V battery or the bench supply's maximum voltage setting. Never connect the same servo signal to the Uno and PCA9685 simultaneously.

Arduino's external-power guidance: https://support.arduino.cc/hc/en-us/articles/360017053760-Troubleshoot-servo-motors

## Finding resting positions

1. Remove discs and disengage the mechanism/load. Connect one servo, then power the Uno and servo supply. Startup sends no position pulses. Opening Serial Monitor may reset the Uno and disable pulses.
2. Send `90` to start at nominal centre. The first command can move abruptly from the unknown physical position. Establish the unloaded travel before mounting the horn to the mechanism; power down before changing the horn or wiring.
3. Send another integer angle, such as `85`. Send `+` or `-` to adjust by one degree. Begin near the centre and approach either end gradually; the full 0..180 command range is not proof that every servo can safely reach it.
4. Record the resting/closed angle and printed `pulse_us`, plus the open/release angle and pulse, against the physical servo's label and intended PCA channel. Repeat separately for every motor.
5. Send `off` to detach the pulse output. It removes holding torque and does not cut electrical power. Turn off the external supply before replacing the servo. There is no automatic timeout; valid angles remain driven until `off` or reset.

`status` shows the last command and whether pulses are enabled. `help` prints commands. Invalid or overlong lines do not move the servo. Commands are lowercase and angles are whole numbers. Records are manual; nothing is stored across reset.

The sketch maps nominal 0..180 to **500..2500 microseconds**, matching the existing project's PCA mapping; 90 gives 1500 microseconds. These are command labels, not measured shaft angles. If those endpoints bind, narrow `MIN_PULSE_US`/`MAX_PULSE_US`; after changing the mapping, transfer the recorded pulse widths rather than assuming the same angle label still matches the ESP32 configuration.

The latest handoff (008) preserves isolation of PCA channels 2–4 after reported full-revolution behavior. Test those suspect servos individually here with no load. If one spins continuously instead of seeking and holding a position, send `off`, remove servo power, and identify whether it is continuous-rotation or damaged. A continuous-rotation servo uses these pulses for speed/direction and cannot be calibrated to a resting shaft angle with this sketch. This test does not re-enable any ESP32 channels.

## Verification

Hardware motion and upload must be checked on the actual Uno and servo. This tool commands position without position sensing.
