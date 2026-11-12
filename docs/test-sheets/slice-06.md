# Test sheet: slice 6 (History and the rollback bar)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 6: History and rollback bar (0.15.0)**, open its Windows
build check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says
0.15.0.

## Set up

Add a Box. Round its four top edges (Edges → Rounding, click the top face, Apply). Add a
Cylinder (radius 5), put it in the middle of the box (X 0, Y 0), select the rounded box
then the cylinder, **Subtract**.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Open the object in the Objects panel | Steps in order: 1. Box 1 (20 × 20 × 20 mm), 2. Rounded 1 (4 edges rounded R2), 3. Subtract 1 (minus Cylinder 1), then the red rollback bar. Cylinder 1 is under step 3 | |
| 2 | Double-click **Box 1**; make it taller in Properties | Still rounded on top, still with the hole | |
| 3 | Double-click **Rounded 1** | The Edges tool opens on it; change the radius, Apply | |
| 4 | Right-click **Subtract 1** → Change to → Intersect, then back to Subtract | Only the overlap, then the hole again | |
| 5 | Right-click **Rounded 1** → Switch this step off | Sharp edges, hole still there; the step shows "(off)" in grey italics. Switch it on again | |
| 6 | Drag the **rollback bar** up to just under Box 1 | The plain box; later steps greyed; the object says "rolled back to step 1 of 3". Split / Mirror / Merge are greyed out | |
| 7 | With it rolled back, select it and **Edges → Chamfer** a vertical edge, Apply | A new step "Chamfered 1" appears as step 2, with the bar under it | |
| 8 | Drag the bar to the bottom (or right-click the bar → Roll forward to the end) | Every step back: chamfer, rounding, hole. If the rounding can't follow the chamfered corner, it shows **⚠** in red with the reason, and the rest still shows | |
| 9 | Hover over a ⚠ step | The tooltip says why | |
| 10 | Right-click the chamfer step → **Delete this step** | Gone; the rest works again. Undo brings it back | |
| 11 | Rolled back, try **Emboss** | The words go in at the bar | |
| 12 | Undo / Redo through all of the above | Each step comes and goes cleanly | |
| 13 | Save, close, reopen (rolled back, with a step off) | Same steps, same bar, same off step | |
| 14 | Merge two separate boxes; open the result | 1. Box 1, 2. Merge 1 with Box 2 under it | |

### Fixes from the first test

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 15 | Box with its top edges rounded: make the box taller 1 mm at a time up to 40 mm (then wider) | Stays rounded at the new top every time | |
| 16 | Then **Change the edges...** on the rounding | The four top edges are already picked | |
| 17 | Select the Subtract step, tick **Through all** in Properties | The cylinder's hole goes right through; make the box taller: still right through | |
| 18 | Untick it | Back to the cylinder's own length | |
| 19 | Lay the cylinder on its side (rotate 90° around X), Through all on | A hole right through sideways | |
| 20 | Open **Emboss** on a box | The words start in the middle of the face you're looking at | |
| 21 | Click near a corner, then **Center on the face: Top-Bottom**, then **Left-Right**, then move and **All** | Each moves the words to the middle that way only (All: both). Turn the words 90° and Left-Right follows the words' direction | |
| 22 | Emboss on a cylinder's side, **Top-Bottom** | Halfway up the side | |

## Send back

"All pass" or the failures (screenshots of the Objects panel help), and `forge3d.log` if
anything failed.
