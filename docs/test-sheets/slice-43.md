# Test sheet: slice 43, Lay out pieces on plates (0.52.0 Beta)

Time needed: about 15 minutes. Your build plate size (Edit > Build Plate Size) is used;
the notes below assume about 220 × 220 × 250 mm.

## Get the build

Open the pull request **Slice 43: Lay out pieces on plates (0.52.0)**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 300 × 150 × 260. Print/Save → **Split to Fit** | The panel has **Then lay the pieces out on plates**, ticked | |
| 2 | Click **Split** | Four pieces, set out side by side on plates drawn beside the first (one piece to a plate at this size), each standing on a cut face: the pieces with pegs have them pointing up. The view zooms out to show them all. No "off the plate" warning | |
| 3 | Undo once | The whole box back in one step; the extra plates go | |
| 4 | Split to Fit again with the box unticked | Pieces cut but left where they were (as in 0.51) | |
| 5 | Add a Cylinder and turn it on its side (Around X 90), plus a few small boxes. Select them all, Print/Save → **Lay Out ▾ → Flattest Side Down** | The cylinder stands on its end; all of them in a neat row on plate 1, not touching | |
| 6 | Turn one shape at an odd angle, then **Lay Out → As They Stand** | They're set out in rows but nothing is turned over (the odd one keeps its angle) | |
| 7 | Add ten 100 × 100 × 20 boxes (Duplicate helps) and Lay Out with nothing selected | Four to a plate: plates 1 and 2 full, two on plate 3 | |
| 8 | With a 300 mm box among them, Lay Out | The big box stays where it was; the status bar says one was too big | |
| 9 | **Export** with shapes on several plates | Asked: **One File per Plate** or **One File**. Per plate: files "… - plate 1", "… - plate 2"... Open two in your slicer: each plate's shapes come in on its plate | |
| 10 | Lay out one of your own split figures (Split to Fit, ticked) | Sensible: every piece on a flat side, pegs up where they can be; note anything you'd change | |
| 11 | Split something with Split → Smart Split, Duplicate, Import… as usual | All still work; Undo after each takes back just that step | |
