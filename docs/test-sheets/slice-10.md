# Test sheet: slice 10, Follow (grooves and ridges along a path) (0.19.0)

Time needed: about 20 minutes. (Slice 9, PR #27, should be merged first: this builds on it.)

## Get the build

Open the pull request **Slice 10: Follow, grooves and ridges along a path (0.19.0)**, open
its Windows build check, download the artifact, unzip and run `Forge3D.exe`. **Help →
About** says 0.19.0.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 40 × 40 × 10. Design → **Follow** | Panel "Groove or ridge on 'Box 1'"; Make (label above the list): Groove; Shape across the path: Round | |
| 2 | Click the middle of the box's top | The sketch view opens ("Draw the path") with the top's outline dashed, the dot where you clicked | |
| 3 | Draw a circle round the dot, about 12 mm across. **Done** | Back in the panel: an orange ring on the top and the preview shows a round groove under it | |
| 4 | Shape across → Square, then V; change Width / Depth | The preview changes each time | |
| 5 | Make → Ridge | The groove becomes a raised ridge (Depth becomes Height) | |
| 6 | Back to Groove, Square; **Apply** | "Grooved 1  square groove, 2 x 1 mm" (your sizes) | |
| 7 | Undo, Redo | The groove goes and comes back in one step | |
| 8 | Make the Box step taller (Height 20) | The groove is still in the top | |
| 9 | Double-click Grooved 1 | The panel opens with the ring shown; **Draw the path...** reopens the circle | |
| 10 | Add a zig-zag of lines to the path (not joined to the circle), Done, Apply | A zig-zag groove as well, with neat corners | |
| 11 | Add a Cylinder (R10, 30 high). Follow, click its curved side halfway up. Draw a straight line across (about 16 mm), Done | The orange path bends round the cylinder (it doesn't run off in a straight line); Apply: a groove round part of it | |
| 12 | Double-click it, **Draw the path...**: make the line long enough to go nearly all the way round (about 60 mm), Done | It wraps almost all the way round | |
| 13 | Shape across → **My own (drawn)** | "Draw the shape across the path first…"; Apply greyed | |
| 14 | **Draw the shape across...**: draw a closed triangle with its point below the dashed line, Done | The groove takes that shape (a V with its top above the surface) | |
| 15 | Click somewhere else on the cylinder | The path moves there | |
| 16 | Follow on a box: draw a path that runs off the face's edge | A clear message, or the path follows over the edge; nothing breaks | |
| 17 | Round, Depth more than Width | "A round groove or ridge can't be deeper than it is wide."; Apply greyed | |
| 18 | Esc / Cancel during each step | Back to how it was | |
| 19 | Save, New, Open | Grooves and ridges come back and can still be changed | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
