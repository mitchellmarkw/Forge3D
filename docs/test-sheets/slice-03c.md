# Test sheet: slice 3c (Merge, Subtract, Intersect, Separate)

Time needed: about 25 minutes.

New: select several shapes and combine them. The result stays editable: open it in the
**Objects** tree to change its parts. **Separate** splits it back into parts.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03c-combine`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.
3. Click **Add Box** twice and **Add Cylinder** once.

## Checks

### Selecting several shapes

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Click Box 1, then **Ctrl+click** Cylinder 1 in the view | Both glow; both are highlighted in Objects; Properties says "2 shapes selected … ('Box 1') is kept" | |
| 2 | **Ctrl+click** Cylinder 1 again | It is removed from the selection | |
| 3 | In the Objects list, click Box 1 then Ctrl+click Box 2 | Both are selected in the view too | |
| 4 | With two selected, look at the toolbar | **Merge**, **Subtract** and **Intersect** are enabled; **Separate** is not | |
| 5 | Select both boxes and press **Delete** | Both disappear; one Ctrl+Z brings both back | |

### Subtract (a hole)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | Select the cylinder; in Properties set X and Y so it sits in the middle of Box 1, and Height to `40` | The cylinder pokes through the box | |
| 7 | Click Box 1, Ctrl+click the cylinder, click **Subtract** | A round hole through the box. The Objects list shows **Subtract 1** (bold) instead of the two. Undo reads "Undo Subtract" | |
| 8 | Select **Subtract 1** | Properties shows "Made from: Box 1 − Cylinder 1", a **Separate into parts** button, Position and Rotation, and no Size | |
| 9 | Orbit to look into the hole from above and below | A clean round hole all the way through | |
| 10 | Try Subtract with the cylinder clicked **first** | The box is cut from the cylinder instead (the first one you click is kept). Undo afterwards | |

### Editing the parts

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 11 | Click the arrow next to **Subtract 1** in Objects, then click **Cylinder 1** under it | The cylinder shows see-through inside the box. Properties says "Part of 'Subtract 1'…" and has no Sit on plate button | |
| 12 | Change its **Radius X** to `8` | The hole gets bigger straight away | |
| 13 | Change its **X** so it sits on the edge of the box | The hole becomes a notch in the side | |
| 14 | Select **Box 1** under Subtract 1 and change its Height | The result gets taller; the hole stays | |
| 15 | With a part selected, press **Delete** | Nothing is deleted; the status bar explains you must delete the whole shape or Separate it | |
| 16 | Ctrl+Z through those edits | The hole returns to how it was, one step at a time | |

### Moving and turning a result

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 17 | Select **Subtract 1** and drag it across the plate | It moves as one piece, hole and all | |
| 18 | Press **E** and drag a ring | It turns as one piece. (It turns around the middle of where the parts were when you combined them.) | |
| 19 | Press **R** | No resize handles appear on a combined shape (resize its parts instead) | |
| 20 | **Sit on plate** after tilting | It rests on the plate | |

### Merge and Intersect

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 21 | Overlap two boxes, select both, **Merge** | One shape with no seam where they overlapped. Objects shows **Merge 1** | |
| 22 | Overlap a box and a cylinder, select both, **Intersect** | Only the shared part is left (e.g. a rounded block) | |
| 23 | Select two shapes that **don't touch** and click **Intersect** | A message: "These shapes don't overlap, so Intersect would leave nothing." Nothing changes | |
| 24 | Select **Merge 1** and Ctrl+click another shape, then **Subtract** | A combined shape can itself be combined; the tree shows it nested | |

### Separate, delete, files

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 25 | Select a combined shape and click **Separate** | The parts come back as separate shapes, exactly where they were (even after moving or turning the result), and are selected | |
| 26 | Ctrl+Z | They join back into the combined shape | |
| 27 | Select a combined shape and press **Delete** | It and all its parts disappear; one Ctrl+Z brings everything back | |
| 28 | Save with a nested combined shape; close; reopen | Everything comes back, still editable, with the same hole and parts | |
| 29 | Open `test2.f3d` from earlier | Opens as before | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` if anything failed, or if any combine took more than a second or two.
- Anything confusing about which shape Subtract keeps.
