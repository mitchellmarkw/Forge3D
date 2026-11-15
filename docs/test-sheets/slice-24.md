# Test sheet: slice 24, Pattern brush (0.33.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 24: Pattern brush (0.33.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere, select it, Sculpt tab → **Sculpt**, Brush **Pattern** | Under the brush description: **Pattern:** (Scales) with its label above it, and **Pattern size** | |
| 2 | Brush Size about 6 mm, drag across the front a few times | Scales pressed into the surface where you dragged; strokes side by side join up | |
| 3 | Drag over the same place several more times | It doesn't get any deeper | |
| 4 | Try Hexagons, Woven cloth, Skin and Bricks, and a bigger and smaller Pattern size | Each pattern as named; size changes how big each repeat is | |
| 5 | Hold **Ctrl** while dragging | The pattern is engraved (goes in) instead | |
| 6 | Pattern → **Your own picture...**, choose a picture (a logo or texture) | Its name appears in the list and it's pressed in where you drag, light parts highest | |
| 7 | Tick **Mirror X**, drag on one side | The pattern appears on the other side too, mirrored | |
| 8 | Paint a **Mask** over part, then drag Pattern across it | Nothing pressed into the masked part | |
| 9 | **Undo stroke** | The last pattern stroke goes | |
| 10 | **Done**, then double-click the Sculpted step and add more pattern | The earlier patterns are there; the new strokes add to them | |
| 11 | Change the Sphere's size inside the step | The patterns are pressed in again on the new sphere | |
| 12 | Save (with your own picture used), close, open | The same shape | |

## Send back

"All pass" or the failures (with the number).
