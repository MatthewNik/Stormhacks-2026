# Preparation-package verification record

## Completed document checks

- Three authoritative Markdown files written; handoff and root README updated to point to them.
- Standalone LaTeX source created with embedded TikZ diagrams and no external image/bibliography requirement.
- PDF exported successfully using the existing project-local Tectonic executable. Final output: 21 pages, 24 unique clickable external links.
- Final PDF rendered with Poppler at 100 dpi. Every page visually reviewed for clipping, overlap, table layout, headers/footers and diagram visibility. Calibration tables kept intact for use as worksheets.
- PyMuPDF checks found no text blocks outside page bounds and confirmed required engine/library API/datasheet link annotations.
- Contents, wiring map, nine servo roles, magazine order, full-column policy and recovery rules checked across Markdown, LaTeX and diagrams.
- Draw.io XML contains three editable pages, unique cell IDs, valid source/target references and vertex geometry. It embeds shapes/connectors, not a flattened PNG. Matching game-flow, magazine and wiring PNGs generated and inspected.
- Following the user's report that the XML imported badly, fixed vertex geometry to use `width`/`height` rather than `w`/`h`. Matched PNG font sizing, fixed connector paths and label positions, supplied explicit diamond fills, and disabled automatic colour conversion. Editor grid is off for clarity; PNG styling/content is retained.
- With explicit user authorization, loaded the corrected three-page file into the online draw.io editor. Visually inspected Game flow, Two-stop magazine, and Connection overview. Double-clicking a game-flow box entered text editing; selecting a magazine connector exposed its line/waypoint settings, confirming separate editable objects. Saved `output/diagrams/drawio_editor_verified.jpg` as browser evidence and kept the editor tab open.
- `output/diagrams/Open in draw.io.url` and `open_editable_diagram.html` contain the diagram data in a draw.io editor link. They launch an editable copy; changes made online must be saved to retain them in the project file.
- Existing firmware file timestamps remain September 24 for the serial sketch/game header and September 21 for the old physical controller. No ESP32 firmware was changed or uploaded.

## Limitations explicitly retained

- Built-in LaTeX source opening was queued. Its diagnostic compiler reported `Unable to find standard directories for platform`. Export succeeded through the existing project compiler; the built-in compiler's environment remains unverified. No compiler/plugin was installed.
- Earlier browser validation was blocked by automatic approval review for lack of specific authorization. The user subsequently explicitly authorized loading and editing the diagram online; actual editor validation is now complete. The browser's native file-chooser/download event paths timed out, so validation used draw.io's documented encoded-diagram URL. Saved each page from the editor's own Edit Diagram XML source into the final local `.drawio` file. A round-trip comparison confirmed identical cell IDs, text, source/target references, and vertex geometry on all three pages. The local file now contains the editor-verified serialization.
- The temporary localhost server used to open the authorized editor link was stopped after validation. No preview service is left running by this task.
- Amazon listing identity/specifications were not independently retrieved. Seller images/descriptions were supplied by the user. Manufacturer datasheets describe the sensor element/servo, not guaranteed characteristics of every marketplace module.
- Physical reliability, signal polarity/voltage, servo endpoints/timing, one-disc isolation and power behavior remain hardware tests. Blank calibration entries and unchecked acceptance boxes are intentional.

## Rebuilding after an approved documentation change

1. Update the relevant authoritative Markdown file. Update the diagram definitions if the flow/wiring changes.
2. Run `python tools/build_hackathon_package.py` to regenerate the standalone LaTeX and editable diagrams.
3. Keep the same source open in the built-in editor. Attempt its compiler only if the environment issue has been resolved; use `tools/tectonic/tectonic.exe` for PDF export as available.
4. Render the new PDF with Poppler into `tmp/pdfs/hackathon-review/final-*.png`, then run `python tools/check_hackathon_package.py` and review every page.
5. Use a fresh review directory if page count decreases, so stale page images do not enter the checks. Repeat actual draw.io validation only with authorized local/offline tooling or user-approved external editor access.