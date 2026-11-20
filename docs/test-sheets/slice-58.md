# Test sheet: slice 58, Hollow with drain holes (0.67.0 Beta)

Time needed: about 15 minutes, plus a resin print (check 11) when you can.

## Get the build

Open the pull request **Slice 58: Hollow (0.67.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere; Features tab → **Hollow** | The panel: Wall thickness 2 mm, Drain holes (Hole size 3 mm, "No holes yet" in orange), Make plugs ticked | |
| 2 | **Place Them for Me** | Two red rods low on the sphere, apart; "2 holes." | |
| 3 | Click the sphere's side; then Remove Last; then Clear | A third rod where you clicked; it goes; all go | |
| 4 | Place Them for Me again, **Apply** | "Hollowed 1 — 2 mm walls, 2 drain holes Ø3" in Objects, and Plug 1, Plug 2 beside it (R1.4 × 2 mm, or your resin peg fit); one Undo takes it all back | |
| 5 | **Section View** through it | Walls about 2 mm all round, the holes going through into the hollow | |
| 6 | Double-click the Hollowed step, walls 1.5, Apply | It changes; no new plugs | |
| 7 | One of your sculpted figures (a big mesh): Hollow, Place Them for Me, Apply | It hollows (a few seconds on the slower machine); fingers and thin parts stay solid | |
| 8 | A Box 30 × 30 × 3 → Hollow | It stays solid (nowhere thick enough) | |
| 9 | Tools → Rescale Design 1:12 → 1:18 with both Keep boxes ticked | Walls and holes keep their size; the plugs too | |
| 10 | Save, close, open | The hollow and its holes are there | |
| 11 | Print a hollowed part in resin | Resin drains; the plugs fit the holes | |
