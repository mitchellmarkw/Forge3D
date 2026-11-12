# Test sheet: slice 6 fixes (0.15.1)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 6 fixes: edges survive resizing, Through all, Emboss centering
(0.15.1)**, open its Windows build check, download the artifact, unzip and run
`Forge3D.exe`. **Help → About** says 0.15.1.

## Set up

As for slice 6: a Box with its four top edges rounded, minus a Cylinder (radius 5) in the
middle.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Box with its top edges rounded: make the box taller 1 mm at a time up to 40 mm (then wider) | Stays rounded at the new top every time | |
| 2 | Then **Change the edges...** on the rounding | The four top edges are already picked | |
| 3 | Select the Subtract step, tick **Through all** in Properties | The cylinder's hole goes right through; make the box taller: still right through | |
| 4 | Untick it | Back to the cylinder's own length | |
| 5 | Lay the cylinder on its side (rotate 90° around X), Through all on | A hole right through sideways | |
| 6 | Open **Emboss** on a box | The words start in the middle of the face you're looking at | |
| 7 | Click near a corner, then **Center on the face: Top-Bottom**, then **Left-Right**, then move and **All** | Each moves the words to the middle that way only (All: both). Turn the words 90° and Left-Right follows the words' direction | |
| 8 | Emboss on a cylinder's side, **Top-Bottom** | Halfway up the side | |

## Send back

"All pass" or the failures, and `forge3d.log` if anything failed.
