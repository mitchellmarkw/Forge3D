# Test sheet: slice 59, Surface stamps (0.68.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 59: Surface stamps (0.68.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box; Features tab → **Stamp** | The panel: Stamp (Rivet), Raised or sunk (labels on top of their drop-downs), Size 3, Height 1.05, Turn, Row spacing 5, "No stamps yet", Apply greyed | |
| 2 | Click the box's face | A gold rivet there; "1 stamp." | |
| 3 | Drag across the face | A row of rivets follows the drag, evenly spaced, the last where you let go | |
| 4 | Remove Last; then Clear | The row goes; then everything | |
| 5 | Try each stamp (Hex bolt, Screw, Stud, Vent, Scale, Stitch), each Raised and Sunk | Each shape; Height jumps to its usual height; sunk ones show red | |
| 6 | Stitch, Sunk, Turn 30, drag a row | Slanted stitches along the row | |
| 7 | Apply | "Stamped 1 — N … raised/sunk" in Objects; one Undo takes it away | |
| 8 | Double-click the Stamped step: add a few, change the size, Apply | It changes | |
| 9 | One of your sculpted figures: a row of rivets or scales over a curved part | They sit on the surface with no gaps | |
| 10 | Save a small shape with Add → Custom Shapes; open Stamp on another shape | It's listed under the built-in stamps; stamping uses it at the stamp's size | |
| 11 | Save, close, open | The stamps are there | |
