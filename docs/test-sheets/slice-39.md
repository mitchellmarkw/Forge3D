# Test sheet: slice 39, Colours (0.48.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 39: Colours (0.48.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Build tab, with nothing selected | A **Colour** button (a palette) in a **Look** group, greyed out | |
| 2 | Add a Box and click **Colour** | 14 colour squares in two rows (point at one for its name), then **More Colours...** and **Automatic** | |
| 3 | Click the red square | The box turns red. Undo: back to its first colour. Redo: red | |
| 4 | Add a Cylinder; select both (Ctrl+click), Colour → yellow | Both turn yellow, in one Undo step | |
| 5 | Colour → **More Colours...** | The colour picker opens with the shape's colour; pick a colour and OK: the shapes take it | |
| 6 | Right-click the cylinder in **Objects** → Colour → green | The cylinder turns green (the right-clicked shape is selected first) | |
| 7 | Select box (first) and cylinder, **Merge** | The merged shape takes the box's colour | |
| 8 | Give the merged shape its own colour, then **Edges**-round one of its edges | The rounded shape keeps the colour (no new colour for the new step) | |
| 9 | Colour → **Automatic** on the merged shape | Back to the box's colour | |
| 10 | Save, close Forge3D, open the file again | Every colour as it was | |
| 11 | Add two shapes in different colours; **Export** both to a 3MF file and open it in your slicer | The slicer shows (or offers) the two colours. Slicers differ in how they use 3MF colours; note what yours does | |
| 12 | **Import** that 3MF back into Forge3D | The shapes come in with their colours | |
| 13 | Switch to the Dark theme and open Colour | The squares still show clearly with a border; the menu text is readable | |
