# Test sheet: 0.78.1, Make Mould: split either way, undercuts, more pieces

Time needed: about 20 minutes.

## Get the build

Open the pull request **Make Mould: split either way, undercuts, more pieces (0.78.1)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a standing figure and a small part (a head or an accessory).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the figure, Print/Save → **Make Mould** | "Split direction:" (Horizontal) with its label right on top; "Cut at (height):"; a line about undercuts; parts of the figure the halves would catch on shown red | |
| 2 | Choose **Vertical (left and right)**, then **Vertical (front and back)** | The yellow face turns upright each time and moves to where the figure is widest that way; the keys stay in its corners; the red undercuts and the line change | |
| 3 | Click **Find Best Split** | After a moment the direction and place change to the one with the least red (for a standing figure, most likely front and back) | |
| 4 | Change **Cut at** a little either way | The red grows or shrinks to match | |
| 5 | **Pieces: 4 (both halves cut again)** | "Second cut direction:" (the other two ways) and "Second cut at:" appear; a second yellow face crosses the first; two more keys in each half; less red | |
| 6 | **Pieces: 3 (one half cut again)** | "Second cut goes through:" appears; the second face only crosses that half; change the half and see it move | |
| 7 | **Middle** | The second cut goes back to the middle of the part | |
| 8 | With a vertical split, click the figure low down (below the middle) | An air vent goes up from there (with a horizontal split, only above the yellow face) | |
| 9 | **Apply** (4 pieces) | Four shapes named by where they are, e.g. "… mould (front, left)", standing on the plate, closed round the figure | |
| 10 | Move the pieces apart and look inside | The figure's shape in each; pegs on one side of each cut fit holes on the other; the spout and vents split between the pieces | |
| 11 | Double-click one piece | The panel opens with its settings; Split direction, Pieces, Second cut direction and Find Best Split are greyed out | |
| 12 | Change the wall thickness, Apply | All four pieces change; they still stand on the plate | |
| 13 | Undo twice | Back to the figure as it was | |
| 14 | A small part: horizontal, 2 pieces, as in 0.78.0 | Works as before; note any red that looks wrong (a place that would come away freely) | |
| 15 | Save, close, open again | The pieces come back the same | |
