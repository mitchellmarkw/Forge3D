# Test sheet: slice 4c (Duplicate, Copy / Paste, Mirror, Group, several at once)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 4c: Duplicate, Mirror, Group (0.9.0)**, open its Windows
build check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.9.0.

## Checks

### Duplicate and Copy / Paste

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box and a Cylinder. Select the box, press **Ctrl+D** | "Box 1 copy" appears to the right of everything, selected. Undo reads "Undo Duplicate" | |
| 2 | Select both (Ctrl+click), click **Duplicate** in the toolbar | Two copies, side by side, to the right of everything | |
| 3 | Select the cylinder, **Ctrl+C**, then **Ctrl+V** twice | Two copies, each further right | |
| 4 | Select the box, **Ctrl+X** | It disappears | |
| 5 | **Ctrl+V** | It comes back where it was | |
| 6 | Merge two shapes, Ctrl+D the result, open the copy in Objects | It holds its own copied parts | |

### Mirror

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 7 | Add a Wedge (tall end at the left). **Mirror → Left / right (X)** | It flips in place: tall end at the right. Name "Wedge 1 (mirrored)" | |
| 8 | **Mirror → Upside down (Z)** on another shape | It flips over and still sits on the plate | |
| 9 | Select a mirrored shape, click **Remove the mirror** (Properties) | It goes back the right way round | |

### Group

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 10 | Select two shapes that don't touch, press **Ctrl+G** | One "Group 1" in Objects, holding both; one colour | |
| 11 | Drag the group, turn it (E), resize it (R) | Both move, turn and stretch together, still apart | |
| 12 | Click **Ungroup** in Properties | Two separate shapes again, where they were | |

### Several shapes at once

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 13 | Select three shapes (Ctrl+click) | Move arrows appear at their shared middle | |
| 14 | Drag an arrow | All three move together; one Ctrl+Z puts them all back | |
| 15 | Press **E** and drag a ring | They turn together around their shared middle | |
| 16 | Press **R** | No resize handles (Group them to resize together) | |
| 17 | With all three still selected, drag one of them on the plate | All three slide | |
| 18 | Click one of them without dragging | Only that one is selected | |
| 19 | Select two shapes and look at Properties | The hint text wraps; the panel doesn't get wider | |
| 20 | Save, close, reopen with a group and a mirrored shape | Both come back as they were | |

## Send back

"All pass" or the failures, plus `forge3d.log` if anything failed.
