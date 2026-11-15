# Test sheet: slice 25, Measure (0.34.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 25: Measure (0.34.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box (40 × 30 × 20) and a Cylinder beside it. Build tab → **Measure** (also on Design and Print) | The Measure panel on the right; the status bar says what to click | |
| 2 | Move the mouse over the shapes | Faces, edges and corners light up orange as you pass them; the panel's grey line says what it is | |
| 3 | Click the Box's top face | It turns red; the panel shows **Flat face: 1200.00 mm²** | |
| 4 | Click the Box's bottom face | **Parallel, 20.00 mm apart**, distance 20.00 | |
| 5 | Click a side face, then the top face | **Angle: 90.0°**, they touch | |
| 6 | Click the Cylinder's round side | **Ø** and radius of the cylinder | |
| 7 | Click a straight edge of the Box | Its length; then a corner of the Box and a corner of the other end: the distance, with X / Y / Z | |
| 8 | Make a Box with two holes (Subtract two cylinders). Click one hole's wall, then the other's | **Centre to centre** (blue line) as well as the distance | |
| 9 | Click a hole's wall, then the Box's side face | **Centre to the face** | |
| 10 | Hold **Shift** and click two spots on a face | Exact points (crosses), and the distance between them | |
| 11 | Import an STL and click on it | Points only, and distances between them | |
| 12 | A third click | Starts again with that one; **Clear** empties it | |
| 13 | While measuring, drag on a shape | Nothing moves or gets selected | |
| 14 | **Done** (or Esc) | Back to normal; nothing in the model has changed (Undo has nothing new) | |

## Send back

"All pass" or the failures (with the number).
