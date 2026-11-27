# Test sheet: Forge3D Scenery 0.5.0 — Trees

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 114: Scenery tree designer (Scenery 0.5.0, Forge3D 0.122.0)**, open its Windows build
check, download `Forge3D-Scenery-windows-x64-…` (and `Forge3D-windows-x64-…` for the last steps).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run Scenery; scale 1:35; New Tree | A panel on the right (New tree); a broadleaf tree with a rounded crown, quick at once, finer a moment later; it fits under the printer (not orange) | |
| 2 | Read under the panel | Its size on the plate, and "Branches thinner than 1.0 mm are thickened to print" | |
| 3 | New Shape a few times | A different tree each time; an earlier Number makes that one again | |
| 4 | Branches 0, then 100 | Fewer, then more branches and clumps | |
| 5 | Foliage 0, then 100 | Sparser, then fuller clumps; untick Leaves: a bare tree, Foliage hidden | |
| 6 | Lean 20 | The tree leans; its foot stays flat on the plate | |
| 7 | Kind: Conifer | A cone-shaped fir, tiers of branches with foliage; sized to fit the printer | |
| 8 | Kind: Dead tree | Bare, gnarled branches, some broken off; no Leaves box | |
| 9 | Kind: Palm | A curved trunk with arching fronds | |
| 10 | Height 3000, Crown width 2500 | It follows | |
| 11 | Esc | The panel closes; nothing made | |
| 12 | New Tree; Add Tree | "Broadleaf" in Objects, flat on the plate | |
| 13 | Double-click it | The panel opens with its settings (Change the tree) | |
| 14 | New Shape; Update Tree | Another tree, its foot in the same place. Ctrl+Z: as it was | |
| 15 | Save; open the file in Forge3D 0.122 | The tree shows; move it, save; open in Scenery again: Change still works, keeping it where you moved it | |
| 16 | Export for Printing (3MF); slice it | Opens in your slicer, flat on its roots (it needs supports under the crown) | |
