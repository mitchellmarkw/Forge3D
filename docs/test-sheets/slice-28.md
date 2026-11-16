# Test sheet: slice 28, Sketch tools (0.37.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 28: Sketch tools (0.37.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, **Sketch**. Look at the sketch toolbar | **Trim** after Point; **Round corner**, **Offset**, **Mirror** after Helper (on a narrow window the last buttons move under **»** at the end) | |
| 2 | Draw a rectangle about 30 × 20. Esc. Click one corner point, **Round corner**, radius 50 | A red message that the radius is too big, saying how much fits; nothing changed | |
| 3 | **Round corner** again, radius 5 | The corner is an arc with an **R5** dimension; the old corner shows as two short dashed lines | |
| 4 | Ctrl+click two more corners, **Round corner**, 3 | Both rounded at once. One **Undo** puts both back; **Redo** | |
| 5 | Double-click the R5, type 8 | That corner's arc grows; the lines still touch it smoothly | |
| 6 | Click any side of the outline, **Offset**, 3 | A copy of the whole outline 3 mm further out, rounded corners included | |
| 7 | Click a side of the original again, **Offset**, -3. **Undo**. Then **Offset** with -30 | -3: a copy 3 mm inside (Undo takes it away again); -30: a message that it's too far, and nothing changes | |
| 8 | Drag a side of the original rectangle | The copies' sides stay parallel to it | |
| 9 | **Finish** (Pad) | A solid frame: the outer outline with the original cut out of its middle | |
| 10 | New sketch: a circle, and a line right across it sticking out both sides. **Trim** (or X); move over the pieces | The piece under the mouse turns red | |
| 11 | Click the part of the line outside the circle on one side, then the other; then the smaller part of the circle | The line ends at the circle; the circle becomes an arc: a closed D shape, no red loose ends | |
| 12 | Esc. **Finish** | A D-shaped solid | |
| 13 | New sketch: Line from the origin straight up; Esc, click it, **Helper** (it goes dashed). Draw half a shape to its right with lines, starting and ending on the dashed line (the grid snaps onto it) | An open outline, its two ends on the dashed line | |
| 14 | Pick the lines of the half shape (Ctrl+click each), Ctrl+click the dashed line, **Mirror** | The other half appears on the left: a closed outline | |
| 15 | Drag a corner of the right half | The left half follows as its mirror image | |
| 16 | **Mirror** with nothing picked; **Offset** with nothing picked | A message saying what to pick | |
| 17 | Finish, Save, close, open, double-click the shape | The sketch as it was, rules included | |

## Send back

"All pass" or the failures (with the number).
