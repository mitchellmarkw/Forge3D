# Test sheet: slice 38, Split for printing (0.47.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 38: Split for printing (0.47.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

### Keyed split

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 60 × 40 × 20. Design → **Split** | At the top a **Smart Split: Fit the Printer** button; at the bottom an **Alignment pegs** group: "Key the cut" ticked, **Pegs on one piece**, Diameter 5, Peg length 6, Taper 2°, Clearance 0.20 | |
| 2 | Choose **Only the top** | The Alignment pegs group greys out. Back to **Both halves** | |
| 3 | **Flat**, in the middle, **Split** | Two shapes; the status bar says 2 alignment pegs. The bottom piece has two pegs standing up out of its cut; the top piece has two holes in its underside (Section View, or turn it over) | |
| 4 | Look closely at a peg | Slightly narrower at its tip than at its foot | |
| 5 | Measure a hole's opening against a peg's foot | The hole is a little wider (0.4 mm across) | |
| 6 | **Undo** | One box again | |
| 7 | Split again with **Loose pins**, Pin length 10 | Holes in both pieces, and two pins standing beside the box | |
| 8 | Add a thin plate (Box 60 × 3 × 20) and split it **Across X** with pegs on | It splits, with a message that the cut has no room for pegs | |
| 9 | Split a shape with a hole through it (or a hollow one) across the hole | The pegs avoid the hole | |
| 10 | Close and reopen Split | Your peg settings are remembered | |

### Split to Fit

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 11 | Edit → Build Plate Size: your printer. Add a Box 400 × 40 × 20 | It's orange-red; the status bar says it's too big for the printer | |
| 12 | Print/Save → **Split to Fit** | "'Box 1' is 400 × 40 × 20 mm, too big for your printer… Split it into 2 pieces (1 cut across X)?", with the peg size | |
| 13 | **Split** | Two pieces, keyed, now amber: the status bar says they're off the plate (not too big) | |
| 14 | Drag one piece onto the plate | Its own colour again | |
| 15 | Undo until it's one box; make it 400 × 300 × 300 (bigger than the printer every way). Split to Fit | It says how many pieces; afterwards every piece is amber, none orange-red | |
| 16 | Two big boxes joined by a thin bar (Merge them), longer than the plate. Split to Fit | The cut goes through the thin bar, not through a box | |
| 17 | Select a shape that fits. Split to Fit | A message that it already fits | |
| 18 | Export one piece as STL and open it in your slicer | The pegs or holes are there; print a pair if you can, and check they go together | |

## Send back

"All pass" or the failures (with the number).
