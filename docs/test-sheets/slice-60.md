# Test sheet: slice 60, Panel lines (0.69.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 60: Panel lines (0.69.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box; Features tab → **Panel Lines** | The panel: Groove shape V (label on top of its drop-down), Width 0.6, Depth 0.5, "No lines yet." | |
| 2 | Click three points on the front face | A red groove line through them; "Drawing one: 3 points." | |
| 3 | Click a point round the corner, on the side | The line goes over the edge onto the side | |
| 4 | **Undo Point**; **End Line**; draw another and **Close Loop** | The last point goes; "1 line." then "2 lines." with the loop joined | |
| 5 | Change Width to 1.5 and Groove shape to Square, then Round | The red lines follow | |
| 6 | Apply | "Panel-lined 1 — 2 panel lines, …" in Objects; the grooves are cut; one Undo takes them away | |
| 7 | Double-click the step: add a line, Apply | It changes | |
| 8 | One of your sculpted figures: lines round an armour plate (Close Loop) | The groove follows the curves at an even depth | |
| 9 | Remove Line, Clear | The line being drawn, else the last one, goes; then all | |
| 10 | Save, close, open | The panel lines are there | |
