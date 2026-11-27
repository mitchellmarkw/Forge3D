# Test sheet: Forge3D Scenery 0.4.0 — Terrain

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 113: Scenery terrain designer (Scenery 0.4.0, Forge3D 0.121.0)**, open its Windows
build check, download `Forge3D-Scenery-windows-x64-…` (and `Forge3D-windows-x64-…` for the last steps).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run Scenery; scale 1:35; New Terrain | A panel on the right (New terrain); a hilly piece filling most of the plate, rough at once, finer a moment later | |
| 2 | Read under the panel | "On the plate: … x … mm, hills up to … mm above the base, at 1:35" | |
| 3 | New Hills a few times | Other hills each time; the Number changes. Type an earlier Number: those hills again | |
| 4 | Hill height 0 | Flat ground (a little rough); then Hill height 1500 | |
| 5 | Untick Level at the edges | The hills run out to the edge; tick it again: they fade to the base's height there | |
| 6 | Shape: Round or oval | An oval piece; Width = Depth makes it round | |
| 7 | Edge: Bevelled; Bevel 4 | The sides slope in like a plinth, smooth all round; Edge Plain again | |
| 8 | Add Crater; click on the ground | A crater (a bowl, a raised rim) where you clicked, picked (an orange column) | |
| 9 | Drag the crater | It follows | |
| 10 | With it picked: Across 3000, Depth 800 | It changes | |
| 11 | Add Crag; click on the ground | A rock outcrop sticking up there | |
| 12 | Add Path; click at one side, then the other | A flattened, slightly sunken track between the two clicks | |
| 13 | Drag the path's end, then its middle | The end moves; then the whole path | |
| 14 | Pick the crag; press Delete | It's gone | |
| 15 | Craters 4, Crags 2, Paths 1; Scatter | They appear at random, "(scattered)" in the list; your crater and path stay | |
| 16 | Scatter again; then Clear Scattered | Other places; then only yours left | |
| 17 | Esc while placing; Esc again | Placing stops; then the panel closes, nothing made | |
| 18 | New Terrain; Scatter; Add Terrain | "Terrain" in Objects | |
| 19 | Double-click Terrain | The panel opens with its settings and features (Change the terrain) | |
| 20 | New Hills; Update Terrain | The same piece, other hills, in the same place. Ctrl+Z: as it was | |
| 21 | Save; open the file in Forge3D 0.121 | The terrain shows; move it, save; open in Scenery again: Change still works, keeping it where you moved it | |
| 22 | Export for Printing (3MF) | Opens in your slicer, its underside flat on the bed | |
