# Test sheet: Forge3D Scenery 0.8.1 — Towers in their own material

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 118: Scenery towers in their own material (Scenery 0.8.1, Forge3D 0.126.0)**, open its
Windows build check, download `Forge3D-Scenery-windows-x64-…`.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run Scenery; New Building; Material: Fieldstone (irregular) | A fieldstone house | |
| 2 | Add Tower; click a corner's post | A round tower in fieldstone (irregular stones all the way round, no blocks) | |
| 3 | Material: Rough-hewn stone | The house and the tower in rough courses | |
| 4 | Material: Brick | Both brick | |
| 5 | Pick the tower: Tower material: Fieldstone (irregular) | A fieldstone tower on the brick house; the list says "fieldstone" | |
| 6 | Tower shape: Square | A square fieldstone tower | |
| 7 | Tower material: Stone blocks; then Logs | Stone blocks; then a plain tower | |
| 8 | Tower material: Same as the building | Brick again | |
| 9 | Add another tower; leave it the same as the building; change the building's material to Stone blocks | That one follows the building; the first keeps its own | |
| 10 | Add Building; save; open it again; Change | The towers' materials kept | |
