# Test sheet: slice 17, sculpting prototype (0.26.0 Beta)

Time needed: about 15 minutes. This is a prototype: besides pass / fail, tell me how it
**feels** (smooth or jerky, too strong or too weak, too slow at High).

## Get the build

Open the pull request **Slice 17: Sculpting prototype (0.26.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere and make it R 20. Sculpt tab → **Sculpt** | The Sculpt panel: Brush (label above) Draw, Size, Strength, Detail (label above) Medium, Undo stroke (greyed), Done, Cancel; "about N triangles" at the top | |
| 2 | Point at the sphere | An orange ring on the surface, as big as Size | |
| 3 | Drag across the sphere | A raised ridge follows the mouse, smoothly | |
| 4 | Hold **Ctrl** and drag | A groove pressed in | |
| 5 | Hold **Shift** and drag over the ridge | It softens and flattens | |
| 6 | Press **]** a few times, then **[** | The ring grows, then shrinks; Size follows | |
| 7 | Strength to 100, then 10, and drag each time | Strong and gentle strokes | |
| 8 | **Undo stroke** (and Ctrl+Z) | The last strokes come off, one at a time | |
| 9 | Turn the view (right button) and sculpt the other side | Works from any side; turning the view doesn't sculpt | |
| 10 | **Done** | The sphere keeps the sculpting; Objects says "N triangles"; Properties "A mesh: N triangles" | |
| 11 | Ctrl+Z | The plain sphere comes back (one step) | |
| 12 | Add a Box (40 × 40 × 20), Sculpt, Detail **High**, drag across the top and over an edge | Works; note whether it's still smooth at High | |
| 13 | **Cancel** (or Esc) | The box as it was | |
| 14 | Sculpt something, Done, then Print → Export STL and open it in your slicer | A clean, watertight model with the sculpting | |
| 15 | How did it feel? Speed at Medium / High, brush strength, anything odd | (your notes) | |

## Send back

"All pass" or the failures (with the number), your notes for 15, and `forge3d.log`
(Help → Open log folder): it records how long each dab took.
