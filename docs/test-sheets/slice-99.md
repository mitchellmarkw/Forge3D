# Test sheet: 0.107.0, Slice 99: Lasso cut and lasso mask

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 99: Lasso cut and lasso mask (0.107.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure, and a plain box.

## Lasso Cut

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the box; Figure Design → **Lasso Cut** | A Lasso cut panel with "What to keep", Clear the Loop, Cut and Cancel | |
| 2 | Drag a loop round one corner of the box | A dashed yellow outline follows the mouse; let go and it closes, lightly filled | |
| 3 | Right-drag to turn the view | The loop stays on its plane in the middle of the box | |
| 4 | What to keep: Cut away inside the loop; **Cut** | The corner is cut away straight through, the way you were looking when you drew it | |
| 5 | Undo | The box whole again, one undo | |
| 6 | Lasso Cut, Keep only inside the loop, a loop round the middle seen from the top, Cut | A column through the box is all that's left | |
| 7 | Undo; Lasso Cut, Both pieces, a loop, Cut | Two shapes, "Box (inside)" and "Box (outside)", fitting together | |
| 8 | Double-click the cut step in Objects | The loop opens as a sketch to change; change it, and the cut follows | |
| 9 | Lasso Cut, a loop off to the side of the box, Cut | A message that the loop doesn't reach the box; nothing changes | |
| 10 | Draw a loop that crosses itself (a figure eight), Cut | A message to draw it round once without crossing | |
| 11 | On the figure, turn to look from the front, lasso round a hand, Both pieces | The hand comes off as its own piece | |

## Lasso masks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 12 | Select the figure, Design → Split by Mask, click **Lasso: draw a loop instead** | No brush ring over the shape | |
| 13 | Drag a loop round the hand from the front | All of the hand is painted, the back too | |
| 14 | Turn off Lasso, brush a little more onto the wrist | Brush and loop together | |
| 15 | Turn Lasso on, Ctrl+drag a loop round the fingers | The fingers are rubbed out | |
| 16 | Undo Stroke twice | The rubbing out, then the brushing, taken back | |
| 17 | Apply | The hand cut off along the paint's edge; its history says "1 lasso" | |
| 18 | Make Part with a lasso round the chest from the front | A part over the chest, front and back | |
| 19 | Sculpt a shape; click **Lasso mask**; drag a loop round the head | The head shows masked; Draw strokes don't change it | |
| 20 | Ctrl+loop over half of the head | That half unmasked | |
| 21 | With Mirror X on, lasso one arm | Both arms masked | |
| 22 | Done; save; open the file | The sculpt step comes back the same | |
