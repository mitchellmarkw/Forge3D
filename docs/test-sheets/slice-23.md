# Test sheet: slice 23, Remesh and Simplify (0.32.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 23: Remesh and Simplify (0.32.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere, select it, Sculpt tab → **Sculpt**. Snake hook a long horn out | A horn with long, stretched triangles up its sides (turn the view to see them) | |
| 2 | Look at the panel | A **Remesh** button with **Triangle size** next to it (already filled in) | |
| 3 | Set Triangle size to about 0.6 and click **Remesh** | After a moment the whole shape is made of even triangles, the horn still there; the triangle count at the top changes | |
| 4 | **Undo stroke** | Back as it was before the remesh | |
| 5 | Paint a **Mask**, then Remesh | The mask is still there, in the same place | |
| 6 | Keep sculpting after the remesh, then **Done** | Strokes work as before; Done keeps it all | |
| 7 | Change the Sphere's size inside the step (click it in Objects): bigger, back, then smaller | The strokes, horn and remesh are done again on the new sphere every time, in the same places | |
| 8 | Select a plain Box, **Simplify** | A message that it has too few triangles to simplify | |
| 9 | Select the sculpted shape (or import a big STL), **Simplify** | A **Simplified** step; Properties shows **Keep: 50 % of the triangles** and the counts before and after; the status bar count drops by about half | |
| 10 | Change Keep to 10 %, then 2 % | Fewer triangles each time; the shape keeps its look at 10 % and gets rougher at 2 % | |
| 11 | Export the simplified shape as STL and open it in your slicer | A clean, closed model (no errors from the slicer) | |
| 12 | **Remove the simplifying** | The full shape comes back | |
| 13 | Use Crease, Snake hook and the mask, Done, save, close and open the file | Everything comes back as it was | |

## Send back

"All pass" or the failures (with the number).
