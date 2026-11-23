# Test sheet: 0.85.0, Slice 76: Display bases

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 76: Display bases (0.85.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure standing on both feet: one jointed figure, and one in one piece.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Jointed figure: Features → **Pose Test**; pose the legs apart a little | The figure posed | |
| 2 | Under Will it stand?, **Make a Display Base...** | A window: Shape (label on top of the box), Margin, Thickness, Bevel, "Under each foot" (label on top), Peg diameter, Peg height, "Make the holes in the feet too", Hole depth | |
| 3 | Leave it Round, holes in the feet ticked; **Make Base** | A round base under the feet, its top where they touch, reaching about 8 mm past them; "Display base" in the Objects list | |
| 4 | Look under each foot (or hide the figure) | A peg standing up from the base right under each foot, and a matching hole up into each sole | |
| 5 | Undo | The base and the holes go together | |
| 6 | Redo; make another base, Square; then one Oval, then one Hexagon | Each the shape asked for, round the same feet; the second doesn't add the foot holes again | |
| 7 | Set Thickness 3 and choose "Holes going down" with Hole depth 3 | It warns the holes must be less deep than the base is thick, and Make Base is greyed out | |
| 8 | Hole depth 2; Make Base | A base with holes going down where the feet are (no pegs, no holes in the feet) | |
| 9 | Pegs taller than the feet's Hole depth (e.g. Peg height 5, Hole depth 4) | A note that the feet won't sit down on the base | |
| 10 | One-piece figure: Pose Test → **Peg Hole in Each Foot** | A hole in each sole (this used to stop after the first) | |
| 11 | One-piece figure: Make a Display Base... | A base with a peg under each foot; feet that have a hole already aren't drilled again | |
| 12 | Done; select the base; Sit on Plate; Print Check | The base on the plate, ready to print | |
| 13 | Print the base and a foot (or check in your slicer) | The pegs go into the foot holes | |
| 14 | Help → Figure, Toy and Kit Tools | Will it stand? mentions Make a Display Base | |
