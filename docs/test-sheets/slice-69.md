# Test sheet: 0.78.0, Slice 69: Make Mould

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 69: Make Mould (0.78.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a small printed part you'd cast (a head, a hand, an accessory), and a figure.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the part, Print/Save tab → **Make Mould** | The panel opens; a see-through box round the part, a yellow parting face across it, four gold keys in its corners, and a red spout with a funnel from the part's top up out of the box | |
| 2 | Look at the parting face | It crosses the part where it's widest; **Where It's Widest** puts it back there after you change it | |
| 3 | Change **Parting height** and **Wall thickness** | The yellow face moves; the box grows or shrinks round the part, the keys staying in its corners | |
| 4 | Click **Place Vents for Me** | Thin red rods go up from the part's other high spots (none on a part with one high spot); the line under the list counts them | |
| 5 | Click the part above the yellow face | A vent goes up from there; **Remove Last** and **Clear** take vents away | |
| 6 | Click the part below the yellow face | A red message: click above the parting face; nothing added | |
| 7 | Choose **The spout** under "Clicking the part places:" and click the part higher up | The spout moves there | |
| 8 | Lower the spout below the face by raising the parting height above it | A red message about the spout; Apply greyed out until it's fixed | |
| 9 | Untick **Keys in the corners** | The gold keys go; tick it again | |
| 10 | **Apply** | Two shapes, "… mould (top)" and "… mould (bottom)", closed round where the part was, the bottom half standing on the plate; the spout's funnel shows in the top | |
| 11 | Move the top half up and away; look into both halves | The part's shape hollowed into each half; pegs standing up on the bottom half's corners, holes in the top half's; the spout and vents go right through the top | |
| 12 | Undo, then Redo | The part comes back as it was in one step; Redo makes the mould again | |
| 13 | Double-click "… mould (top)" in the Objects list | The panel opens again with the settings it was made with, the part shown inside the box | |
| 14 | Change the wall to 6 mm, Apply | Both halves change; the bottom half still stands on the plate | |
| 15 | Save, close, open the file again | Both halves come back the same | |
| 16 | Export the two halves to your slicer (top half laid upside down) | They slice without errors; print and try a cast if you like (note how the halves line up) | |
| 17 | Help → Figure, Toy and Kit Tools | A Make Mould section under "Getting it ready to print" | |
