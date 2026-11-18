# Test sheet: slice 42, Split to Fit shows its cuts first; angled cuts (0.51.0 Beta)

Time needed: about 15 minutes. Your build plate size (Edit > Build Plate Size) is used
throughout; the sizes below assume about 220 × 220 × 250 mm.

## Get the build

Open the pull request **Slice 42: Split to Fit shows its cuts first; angled cuts
(0.51.0)**, open its Windows build check, download the `Forge3D-windows-x64-…`
artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box; set it to 300 × 20 × 260 mm. Print/Save → **Split to Fit** | Instead of a question, a panel on the right: the size, your printer, "2 cut(s), making about 4 pieces", and the cuts listed. In the view: one yellow plane (with an arrow) and one blue plane through the box. "✓ Every piece will fit the printer." | |
| 2 | Click "Cut 2" in the list | That plane turns yellow (with the arrow); the other turns blue | |
| 3 | Drag the yellow plane in the view | It slides through the box; Position follows. Drag it far to one side: a red warning says a piece could be too long. Drag it back: the green tick returns | |
| 4 | Click the blue plane in the view | It becomes the picked one (yellow), ready to drag | |
| 5 | Type a Position, set **Lean** to 20 and change **Lean around** | The picked plane moves and tilts as you change them | |
| 6 | Press Esc during a drag | The plane goes back to where it was | |
| 7 | **Plan Again** | Your changes are forgotten; the cuts are placed as at first | |
| 8 | Click **Split** | 4 pieces, keyed with pegs, selected; one Undo puts the box back | |
| 9 | Split to Fit again; move a cut so a piece is too long, then click Split | It asks "Split anyway?". No leaves things as they are; Yes splits | |
| 10 | Split to Fit, then **Cancel** (and once with Esc) | The panel closes, nothing is cut | |
| 11 | Make a long bar leaning in the air: a Box 300 × 20 × 20, **Around Y** −25°, Z up 60. Split to Fit; tick **Let cuts lean (angled cuts)** | The cut leans with the bar (Lean shows −10 to −30°), straight across it. Untick: level again | |
| 12 | Split one of your figures (or a big model) to fit, with and without angled cuts | Sensible cuts; note anything you'd move, and whether dragging them is easy | |
| 13 | After the split in check 8, look at the pieces where the two cuts meet (Section View helps) | No peg or hole is cut in half by the other cut | |
| 14 | Design → Split → **Smart Split** (with a big shape) | Opens the same Split to Fit panel | |
| 15 | With Split to Fit open, the status bar | It doesn't count the planes or arrow as shapes off the plate | |
