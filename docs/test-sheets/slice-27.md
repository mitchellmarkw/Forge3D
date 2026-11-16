# Test sheet: slice 27, Loft and sweep (0.36.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 27: Loft and sweep (0.36.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, **Sketch**. Draw a rectangle about 20 × 20 round the origin. In **Make:** (label right above the list) pick **Loft** | "Outline 1 of 2", Below greyed, **Add one above ▶**, Straight sides, and a note on how it works | |
| 2 | Click **Finish** now | A message that a loft needs at least two outlines; still sketching | |
| 3 | **Add one above ▶** | "Outline 2 of 2", Height 20 mm; the rectangle drawn again, and in grey behind it | |
| 4 | Delete the rectangle; draw a circle radius 10 at the origin. Height 30. **Finish** | A shape blending from the square base to a round top, 30 high; Objects: "Loft 1  2 outlines, 30 mm high" | |
| 5 | Double-click Loft 1. **Above ▶**, then **Add one above ▶** (outline 3, height 60). Make that circle smaller; **Finish** | The loft carries on up to the smaller circle (3 outlines) | |
| 6 | Double-click again, go to outline 3, **Remove this outline**, tick **Straight sides**, **Finish** | Back to two outlines, with flat faces between them instead of a smooth blend | |
| 7 | Nothing selected, **Sketch**. A circle radius 2 at the origin. **Make: Sweep**, **Along:** (label above the list) **A path you draw**. Click **Draw the path ▶** | The circle shows edge-on as a grey line, with a grey guide going up; Line tool on | |
| 8 | From the origin, a line straight up 20, then one across 15; Esc. **Finish** | A pipe going up and bending over, the corner rounded; "Sweep 1  swept along a path" | |
| 9 | Double-click Sweep 1, **◀ Back to the outline**, make the circle radius 3; **Finish** | A thicker pipe on the same path | |
| 10 | New sketch: a circle radius 1, centred 10 to the right of the origin. **Make: Sweep**, **Along: A spiral**, Pitch 4, Turns 5, **Finish** | A spring standing on the plate, 5 turns; "spiral, 5 turns" | |
| 11 | Double-click it: tick **left-hand**, then set Pitch 1.5 and Finish | Left-hand is fine; pitch 1.5 gives a message that the turns would touch | |
| 12 | Select the loft, **R** (resize). With **Uniform** off, drag a side handle; then press **U** and drag a corner handle | Stretching one way: a message that a loft or a sweep can only be resized evenly. Uniform: it grows evenly | |
| 13 | Subtract a cylinder from the loft, then round its top edge (Edges) | Both work like any other shape | |
| 14 | Save, close, open | The loft, pipe and spring as they were | |

## Send back

"All pass" or the failures (with the number).
