# Test sheet: slice 4a (Split)

Time needed: about 20 minutes.

New: **Split** cuts a shape in two with a plane, as in 3D Builder. Halves stay editable,
and you can move the cut later.

## Get the build

1. On GitHub, open the pull request **Slice 4a: Split (0.7.0)**, open its Windows build
   check, and download the artifact `Forge3D-windows-x64-…` (or Actions → Windows build →
   newest green run for branch `slice-04a-split`).
2. Unzip it and run `Forge3D.exe`. **Help → About** should say 0.7.0.

## Checks

### Starting and cancelling

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box. With nothing selected, look at **Split** in the toolbar | Greyed out | |
| 2 | Select the box, click **Split** | A see-through yellow plane through the middle of the box, with an arrow pointing up. The right panel becomes "Split 'Box 1'": Direction, Position, Keep, Split / Cancel. The move arrows disappear | |
| 3 | Drag the Position slider end to end | The plane slides from the bottom of the box to its top | |
| 4 | Press **Esc** | The plane goes away and Properties comes back; nothing changed (no Undo entry) | |
| 5 | Click Split again, then click **Cancel** | Same as Esc | |

### Cutting

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | Split, leave everything as it is, click **Split** | Two shapes: "Box 1 (top)" and "Box 1 (bottom)", both selected, touching. Undo reads "Undo Split Box 1" | |
| 7 | Drag the top half away with the Move tool | A clean flat cut on both halves | |
| 8 | Ctrl+Z twice | The move undoes, then the whole box comes back as one | |
| 9 | Split, click **Across X**, choose **Only the right (arrow side)**, click Split | Only the right half is left, named "Box 1 (right)" | |
| 10 | Ctrl+Z. Split, set **Tilt around X** to `30` and **Tilt around Y** to `20`, click Split | A slanted cut. Orbit to check it | |
| 11 | Split a cylinder with **Across Y** | Front and back halves, named "(front)" and "(back)" | |
| 12 | Split, drag the slider all the way to one end, click Split | A message that the plane doesn't cut through the shape; the tool stays open so you can move it | |

### Changing a half later

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 13 | Select a half | Properties shows "Made from: Box 1, cut by a plane", **Remove the cut** and **Move the cut...** | |
| 14 | Click **Move the cut...**, set "From the middle" to `5`, click **Apply** | That half gets thinner (or thicker) in place; the other half doesn't change. Undo reads "Undo Move cut" | |
| 15 | Turn the half with the Rotate tool, then **Move the cut...** again | The plane appears turned with the half, at its cut. Apply works | |
| 16 | Open the half in Objects and select the box inside it; change its Height | The half updates (still cut where it was) | |
| 17 | Select a half and click **Remove the cut** | The whole box comes back where the half was | |

### Combined shapes and files

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 18 | Merge a box and a cylinder, then Split the result flat, keeping both | Two halves; Objects shows the bottom one holding "Merge 1 copy" with its own copied parts | |
| 19 | Save with some halves, close, reopen | Halves come back the same, and Move the cut still works | |
| 20 | Open a file saved with 0.6.1 | Opens as before | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` if anything failed.
- Whether you'd like to drag the plane in the view as well as with the slider.
