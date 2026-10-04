# Full hardware wiring and upload

Deploy from [working code](../working%20code/README.md). Its compile script now always sets MENU_TEST_ONLY=0, and packaging rejects build metadata that does not confirm the full hardware flag. Its deployment guide contains full-version upload commands only. The separate button-test and RPI IMPLEMENTATION directories were not changed during this update.

See [current wiring](../working%20code/WIRING.md) for the complete column/PCA/IR/angle table, power/OE, OLED and button connections. The old planning wiring document's SH1106 OLED and PCA8 second magazine stop are superseded; its seven IR assignments remain current.

Viewed from the front, column 1 is rightmost/PCA0/IR GPIO34, then columns 2–7 use PCA1–6 and GPIO35,36,39,32,33,27. Motor angles and all seven enabled outputs remain as in handoff 009. PCA7 loads at 80 and releases at 145 degrees, independently of the 50 ms-staggered flap batches. Human moves remain terminal-entered and robot moves IR-confirmed, as specified by handoff 004.

Added a regression that drives each physical IR GPIO and verifies its captured column context. All five firmware suites and all 19 Pi tests passed. Full ESP32 compilation passed: 338356 bytes flash, 33876 bytes RAM. The deployment binary was repackaged; full-hardware build metadata, SHA256 checksums and Pi ZIP contents were verified. Packages preserve the remote `.env`, virtual environment, logs and audio settings.

No hardware upload was performed. Follow [DEPLOY.md](../working%20code/DEPLOY.md): stop/quit the Pi program, transfer full-update artifacts from Windows, verify checksums in Pi SSH, extract Pi source, install requirements, and flash ESP32 through its CP2102 USB path with servo power off. PCA9685 must be connected for normal startup. Clear physical board/indexer/feed path and enter confirm-clear inside the running Pi program before menu navigation. Validate unloaded motion and real IR behavior before loaded play.
