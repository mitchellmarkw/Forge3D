# Test sheet: slice 4b (handles on parts and combined shapes)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 4b: handles everywhere (0.8.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.8.0.

## Set up

Add a Box (20 × 40 × 20) and a Cylinder (radius 5, height 15) and put the cylinder on top
of the box (Z = 20). Select the box, Ctrl+click the cylinder, **Merge**.

## Checks

### Parts inside a combined shape

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Open **Merge 1** in Objects and click the cylinder inside it | The cylinder shows see-through, with move arrows on it | |
| 2 | Drag its red arrow | The peg slides along X on top of the block; the merged shape updates as you drag | |
| 3 | Press **E** and drag a ring | The peg turns about its own middle | |
| 4 | Press **R** and drag its top handle | The peg gets taller | |
| 5 | Ctrl+Z three times | Each drag undoes in one step | |
| 6 | Turn **Merge 1** 90° around Z (Properties), then select the peg again and drag its red arrow | The peg still moves along the plate's X (the red line), not the shape's own X | |
| 7 | Start dragging a peg handle and press **Esc** before letting go | It snaps back | |

### Resizing a combined shape

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 8 | Ctrl+Z back to the start. Select **Merge 1**, press **R** | Resize handles on all six sides of the whole shape | |
| 9 | Drag the top handle up | Block and peg both get taller; the bottom stays on the plate | |
| 10 | Drag the +X side handle out | The shape gets wider; the -X side stays put; the peg becomes oval | |
| 11 | Ctrl+Z, turn **Uniform** on, drag the +X handle again | Everything grows evenly (status shows "uniform") | |
| 12 | Turn Uniform off. Turn the peg 30° around X (select it, Properties), then select Merge 1 and drag the top handle | It scales evenly; the status bar says a part is turned at an angle | |
| 13 | Split a box flat, select a half, press **R**, drag its top handle | The half stretches; its cut face stays flat | |
| 14 | Save, close, reopen | Everything comes back as it was | |

## Send back

"All pass" or the failures, plus `forge3d.log` if anything failed.
