# Test sheet: slice 8, Revolve, Hole, Shell (0.17.0)

Time needed: about 25 minutes.

## Get the build

Open the pull request **Slice 8: Revolve, Hole, Shell (0.17.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.17.0.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, **Sketch**. Draw a rectangle to the right of the green (up) axis, from the red axis upwards. **L**: a line from the origin straight up. Esc, Esc, click that line, **G** | The line turns dashed (a helper line) | |
| 2 | In **Make:** (label right above the list) pick **Revolve**. Angle 360, **Finish** | A tube standing on the plate, its hole the gap you left from the axis; Objects: "Revolve 1  sketch, spun 360°" | |
| 3 | Double-click Revolve 1 in Objects, set Angle 180, Finish | Half a tube | |
| 4 | Sketch again with the rectangle crossing the helper line, Revolve, Finish | Refused: "The outline crosses the axis it spins around…"; the sketch stays open | |
| 5 | Sketch with two helper lines and none picked, Revolve, Finish | Asks you to pick the one to spin round | |
| 6 | Add a Box 40 × 40 × 20. Select it, **Hole**, click its top near a corner | Panel "Holes in 'Box 1'": an M3 hole appears where you clicked; the spot is listed (Across / Up in mm from the top's middle) | |
| 7 | Click three more spots on the top | Four holes, all listed | |
| 8 | Click the side of the box | "Click on the same flat face…"; no hole added | |
| 9 | Type 15 / 15 for the first spot | That hole moves there | |
| 10 | **Size** M5, **Made for**: cutting a thread | Diameter 4.20 (tap drill); a bolt to pass through: 5.50 | |
| 11 | **Type** Counterbored, then Countersunk | Each shows its extra sizes (filled in for M5) and the preview changes: a flat step, then a cone | |
| 12 | Type your own Diameter | Size goes to Custom | |
| 13 | Tick **Through all** | Depth greys out; the holes go right through | |
| 14 | Pick a spot in the list, **Remove** | That hole goes | |
| 15 | **Place exactly...** | The sketch view opens with the top's outline dashed and the holes as points; **Point** (P) adds one. Pick a point and the origin, Shift+H, type 10. **Done** | Back in the Hole panel, the spots updated (one at 10 across) | |
| 16 | **Apply** | "Drilled 1" with the holes; Objects says e.g. "3 holes M5 countersunk, through all" | |
| 17 | Undo, Redo | The holes go and come back in one step | |
| 18 | Double-click Drilled 1, change Depth / Size, Apply | The holes change | |
| 19 | Make the drilled box taller (its Box step in Objects, Height 40) | With Through all: holes still go right through | |
| 20 | Add a Box. Select it, **Shell**, click its top | The top's edges turn red; the preview is a cup with 2 mm walls (about a tenth of the smallest side) | |
| 21 | Click the top again; then click a side | Top closes again (sealed hollow); then the side opens | |
| 22 | Tick **Walls outside** | The walls grow outwards instead | |
| 23 | Set Wall thickness very large (e.g. half the box) | A message: "…Try thinner walls."; Apply greyed | |
| 24 | Back to 2 mm with the top open, **Apply** | "Shelled 1  hollowed, walls 2 mm, 1 face open" | |
| 25 | Make the box under it taller (Box step, Height + 20) | Still a cup, open at the new top | |
| 26 | Double-click Shelled 1 | The panel opens with the top already picked | |
| 27 | Select a box, Sketch on its front face, draw a circle and a helper line beside it, Make **Groove**, Finish | A ring-shaped groove cut into the face | |
| 28 | Save, New, Open | Revolves, holes and shells come back and still change by double-clicking | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
