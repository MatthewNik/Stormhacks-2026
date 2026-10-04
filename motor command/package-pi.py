from pathlib import Path
import sys
import zipfile
root = Path(__file__).resolve().parent
workspace = Path(r"C:\Users\matth\Documents\Stormhacks2026 Oct 3-4").resolve()
base, output = (Path(arg).resolve() for arg in sys.argv[1:])
if not root.is_relative_to(workspace) or base != root / "pi" or output != root / "deployment/pi-update.zip":
    raise ValueError("Package paths must stay inside this program.")
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
    for name in ("motor_terminal.py", "requirements.txt"):
        archive.write(base / name, name)
