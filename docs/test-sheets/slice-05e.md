# Test sheet: slice 5e (Edges: rounding, fillet, chamfer, bevel)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 5e: Edges (0.14.0)**, open its Windows build check,
download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.14.0.

## Checks

### Picking

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, select it, click **Edges** | The Edges panel; Rounding chosen, a radius filled in | |
| 2 | Move the mouse over the box's edges | Each edge lights up orange as you pass it; edges round the back stay dark | |
| 3 | Click an edge | It turns red; the view shows it rounded; "1 edge picked" | |
| 4 | Click the same edge again | Dropped: sharp again | |
| 5 | Click the top face | All four top edges picked and rounded. Click it again: all four dropped | |
| 6 | Click **All edges** | Every edge rounded. **Clear** drops them all | |
| 7 | Pick a few edges, change the Radius | The view follows | |
| 8 | Set a radius bigger than the box | A red message; Apply greyed out | |
| 9 | Click **Apply** (with a sensible radius) | Done: Properties says "Made from: Box 1, with N edges rounded R..." | |

### The four kinds

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 10 | Make a plate (Box 40 x 40 x 5) and a Cylinder standing on it; Merge them; **Edges → Fillet** | Only the inside corner where the peg meets the plate lights up. Pick it, Apply: a smooth curved join | |
| 11 | On the same shape choose **Rounding** | Only outside edges light up (the plate's rim, the top of the peg) | |
| 12 | **Chamfer** a box edge 2 mm | A flat 45° cut, 2 mm back on both faces | |
| 13 | **Bevel**: click the top face of a box (width 6, angle 30) | Each top edge gets a slope reaching 6 mm in across the top, dropping 30° from it | |
| 14 | Bevel again, but click the front face instead | The slope is measured from the front face this time | |
| 15 | Pick edges for Rounding, then switch to Fillet | Edges that don't suit are dropped, with a message | |

### Afterwards

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 16 | Select a result, **Change the edges...**: change kind, size, picks; Apply | Changes in place; Undo goes back | |
| 17 | **Sharp edges again** | The plain shape is back | |
| 18 | Move and rotate a rounded shape; resize it with Uniform on | Stays rounded; resized evenly the rounding grows too | |
| 19 | Round a box's top edges, then select the box inside it (as in 4b) and make it taller in Properties | Still rounded at the new top | |
| 20 | Try Edges on an imported mesh | A message: it's for solid shapes | |
| 21 | Save, close, reopen | All rounded / chamfered shapes come back the same, still editable | |
| 22 | Export a rounded shape as 3MF, open it in the slicer | Smooth curves, no repair warnings | |

## Send back

"All pass" or the failures (screenshots help), and `forge3d.log` if anything failed.
