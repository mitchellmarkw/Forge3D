# Test sheet: slice 19, detail where you sculpt (0.28.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 19: Detail where you sculpt (0.28.0)**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere (R 20), Sculpt | Detail (label above) now has Low / Medium / High; "Add detail where I sculpt" ticked; Finest detail in mm | |
| 2 | Detail **Low**, then point at the sphere and move around | The ring follows the mouse without lag | |
| 3 | Draw with a small brush (Size 1 mm), a few strokes | Crisp, fine strokes even though the sphere started coarse; the triangle count at the top goes up as you sculpt | |
| 4 | Untick "Add detail where I sculpt", draw with Size 1 | Coarser, blockier strokes (no detail added); tick it again | |
| 5 | Make Finest detail bigger (e.g. 0.5) and draw | Softer, coarser strokes; the count goes up more slowly | |
| 6 | Undo stroke a few times | The strokes and their added detail come off | |
| 7 | Detail **High** on a Box (40 × 40 × 20): move the mouse around and sculpt | The ring keeps up; sculpting stays smooth | |
| 8 | Try the other brushes and Mirror with detail on | All still work | |
| 9 | Done, then Export STL and open it in your slicer | Watertight, with the fine detail | |
| 10 | How did it feel compared with before? | (your notes) | |

## Send back

"All pass" or the failures (with the number), your notes for 10, and `forge3d.log`.
