# Three-button menu controls

## Current version

Use the firmware in `RPI IMPLEMENTATION/firmware/ConnectFourGameTest`. The original `Arduino/ConnectFourGameTest` remains unchanged. This handoff replaces the two-button mappings in handoff 006; its Pi service, USB protocol, coaching architecture and physical limitations still apply. See the [integration guide](../RPI%20IMPLEMENTATION/README.md).

## Wiring and behavior

| Physical button | ESP32 pin | Action |
| --- | --- | --- |
| Left | GPIO13 to GND | Previous menu option, wrapping around |
| Right | GPIO14 to GND | Next menu option, wrapping around |
| Centre | GPIO23 to GND | Confirm highlighted option and advance |

All inputs use internal pull-ups and active LOW detection. `GameConfig.h` names them `BUTTON_LEFT`, `BUTTON_RIGHT` and `BUTTON_CENTRE`. Confirm physical wiring with USB and servo power disconnected.

- After terminal `confirm-clear`, Mode defaults to Free Play. Left/Right toggle Free Play and Coach; browsing does not start gameplay or change the committed mode. Centre confirms it.
- Free Play proceeds to Easy/Medium/Hard, default Easy, then Human/Robot first, default Human. Centre confirms each screen; the final confirmation starts play.
- Coach confirmation starts the existing human-first, medium-difficulty lesson.
- Each stable press acts once after 30 ms debounce. There are no hold mappings or auto-repeat. Overlapping button presses are ignored until all buttons are released. Every new screen also requires all three released before accepting a new press, preventing a held Centre from advancing twice.
- Buttons operate setup menus only. Centre cannot acknowledge board clearing, commit human moves or invoke recovery. Existing terminal setup/clearing/recovery commands and USB protocol remain compatible.

## OLED

Menus show a small heading, a size-2 highlighted option, `<  Left/Right  >`, and `Centre: confirm`. Free Play is split into large `FREE` and `PLAY` lines because the full label would exceed 96 pixels at size 2. Gameplay and fault/recovery displays are unchanged.

## Verification

- All four native suites passed, including three-button navigation/wraparound in both directions, defaults, human/robot starter paths, Coach confirmation, debounce, no repeat while held, all button-pair chords, release between screens, and ignoring buttons outside setup menus. Existing sensor, actuator, recovery and protocol tests also passed.
- All 19 Pi tests passed, including cross-language validation of actual C++ snapshots. The Pi service required no changes.
- ESP32 Dev Module compile passed: **338360 bytes flash**, **33876 bytes global RAM**. Outputs remain under `RPI IMPLEMENTATION/build/esp32`. No upload performed.
- Static bounds checks verified all menu labels fit the 96x64 display using the default GFX 6x8 font cells, including size-2 labels and the two-line Free Play layout.

## Next operator checks

With servo power off, verify Left/GPIO13, Right/GPIO14 and Centre/GPIO23 wiring, all OLED selections, wraparound and confirmation. Hold each button to check that it acts once; hold Centre through a transition and confirm the next screen requires release and a fresh press. Verify Centre does nothing on the clearing screen or during gameplay/recovery. Physical buttons, actual OLED appearance and loaded mechanism behavior remain unvalidated. Continue the unloaded checks and PCA2–4 isolation requirements from prior handoffs before any loaded run.
