# Test sheet: slice 9, middle planes and the ribbon (0.18.0)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 9: middle planes and the ribbon (0.18.0)**, open its Windows
build check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says
0.18.0.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Look at the top of the window | Tabs **Build / Design / Sculpt / Print**; each tab's buttons in groups with small names under them (File, Undo, Add, Handles, Combine, Arrange, Remove on Build). Nothing cut off at the right | |
| 2 | Click each tab | Design: Sketch (with an arrow), Hole, Shell, Edges, Split, Emboss, Undo. Sculpt: a note that brushes come later. Print: Export, Send to Slicer, Undo | |
| 3 | Use a few shortcuts from another tab (e.g. Ctrl+Z, Delete) | They still work, whichever tab is showing | |
| 4 | Close and reopen Forge3D | It opens on the tab you last used | |
| 5 | Add a Cylinder (R10, 20 high). Design → Sketch's **arrow** | A list: A face (click it), or the plate / Front / Side / Top plane through the middle | |
| 6 | Pick **Front plane through the middle** | The sketch opens with the cylinder's cross-section dashed (a 20 × 20 square round the origin) | |
| 7 | Draw a small rectangle across the square's right-hand edge (about 2 × 2 mm). Draw a line from the origin straight up; Esc, Esc, click it, **G** (helper). Make **Groove**, Finish | A groove cut all the way round the cylinder | |
| 8 | Double-click the Grooved step | The sketch opens again with the cross-section dashed and the helper line picked | |
| 9 | Change the rectangle's height with a dimension, Finish | The groove gets wider / narrower | |
| 10 | Select the cylinder, Sketch arrow → **Side plane**; draw a circle near the middle; Make **Pocket**, tick **Both ways**, Depth 30 | A round hole straight through the cylinder from side to side | |
| 11 | With nothing selected, Sketch arrow → **Front plane**; draw a closed outline above the red line; Pad | The pad stands up from the plate (drawn as seen from the front), pushed towards you | |
| 12 | Sketch on a face (Sketch button itself, then click a face), as before | Works as in 0.16 / 0.17 | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
