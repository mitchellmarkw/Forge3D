# Test sheet: Forge3D Scenery 0.2.0 — Building designer

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 111: Scenery building designer (Scenery 0.2.0, Forge3D 0.119.0)**, open its Windows
build check, download `Forge3D-Scenery-windows-x64-…` (and `Forge3D-windows-x64-…` for the last steps).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run Scenery; scale 1:35; New Building | A panel on the right (New building); a brick house with a gable roof on the plate | |
| 2 | Wait a moment; zoom in on a wall | Bricks in running bond, the mortar set back; the corners bonded (bricks alternate round them) | |
| 3 | Read under the panel | "On the plate: 171.4 x 114.3 x 80.0 mm walls, at 1:35"; a note that the mortar is widened to 0.40 mm | |
| 4 | Smallest detail 0.1 | The note goes (0.29 mm mortar prints on a resin printer) | |
| 5 | Width 8000, Depth 5000, Wall height 3000 | The house follows: plain at once, bricks a moment later | |
| 6 | Untick Mortar | The bricks laid tight, fine joint lines; tick it again | |
| 7 | Material: Stone blocks | Bigger blocks of varied lengths; the sizes change to a block's | |
| 8 | Material: Logs | A log cabin, the logs crossing past the corners; no wall thickness box (the logs' diameter is it) | |
| 9 | Material: Wood planks | Clapboard planks with corner boards | |
| 10 | Back to Brick; Roof: Hip, Flat, None, Gable | Each shown; Pitch and Overhang hidden for None | |
| 11 | Add Door; click low on the front wall | A door there (bottom on the ground), framed, picked (orange) | |
| 12 | Add Window; click on the right wall | A window there, middle where you clicked, with a sill and bars | |
| 13 | Drag the window along its wall, then round the corner to the front | It follows, onto the front wall; it won't go on top of the door | |
| 14 | With the window picked: Width 1200, Sill height 600 | It changes; the list says its size | |
| 15 | Pick the door; press Delete | It's gone | |
| 16 | Esc | The panel closes; nothing made | |
| 17 | New Building again; add a door; Add Building | Two shapes in Objects: Building and Building roof | |
| 18 | Double-click Building | The panel opens with its settings and door (Change the building) | |
| 19 | Material Logs; Update Building | The same building, now logs, in the same place | |
| 20 | Ctrl+Z | Brick again | |
| 21 | Untick "The roof its own shape"; Update | One shape (the roof joined) | |
| 22 | Save; open the file in Forge3D 0.119 | The building shows; move it, save; open in Scenery again: Change Building still works | |
| 23 | Export for Printing (3MF) | Opens in your slicer | |
