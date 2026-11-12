# Test sheet: fillets on outside edges (0.14.1)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Fillets on outside edges: coves (0.14.1)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.14.1.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, **Edges → Fillet**, move the mouse over its edges | Outside edges now light up too | |
| 2 | Pick one top edge, radius 2 | A quarter-round hollow along the edge, as if a 2 mm cylinder centred on the edge were taken away | |
| 3 | Click the top face (all four top edges) | Hollows all round the top, joining cleanly at the corners | |
| 4 | Add a Cylinder; Fillet its top rim | A hollow running all the way round the rim | |
| 5 | Make a radius bigger than the cylinder's own radius | A red message; Apply greyed out | |
| 6 | Peg on a plate (Merge); Fillet both the inside corner round the peg and the plate's outer top edges | The corner is filled, the outer edges get hollows, in one go | |
| 7 | Apply, then **Change the edges...** and change the radius | Updates in place; Undo works | |
| 8 | Save, reopen; export 3MF and open it in the slicer | Same shape; no repair warnings | |
| 9 | Rounding, Chamfer and Bevel | Work as before (spot-check one each) | |

## Send back

"All pass" or the failures, and `forge3d.log` if anything failed.
