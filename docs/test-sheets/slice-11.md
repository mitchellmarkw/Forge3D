# Test sheet: slice 11, Patterns (0.20.0)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 11: Patterns (0.20.0)**, open its Windows build check,
download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.20.0.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 60 × 40 × 10. **Hole**: one M3 hole near the left of the top, Apply | Drilled 1 with one hole | |
| 2 | Design → **Pattern** | Panel "Pattern 'Drilled 1'": Repeat (label above): "Its last step: Hole 1"; Pattern: In a row; the preview shows 3 holes in a row | |
| 3 | Change How many and Spacing; tick The other way; change Direction | The preview follows each change | |
| 4 | Pattern → **Round a circle**; set the centre to the top's middle (X 0, Y 0); How many 6 | 6 holes round the middle | |
| 5 | Angle 90 | The 6 spread over a quarter turn instead | |
| 6 | Back to 360°, **Apply** | "Pattern 1  circle of 6 (Hole 1)" | |
| 7 | Double-click the Drilled step (inside Pattern 1), change the size to M5, Apply | All 6 holes become M5 | |
| 8 | Double-click Pattern 1, How many 8, Apply | 8 holes | |
| 9 | Make the Box step 80 wide | The holes stay round the (moved) middle of the top | |
| 10 | Undo / Redo | One step each | |
| 11 | Add a Sphere. Pattern → Repeat "The whole shape" (the last-step choice is greyed: a sphere has none); **In a grid**, 3 × 2 | 6 spheres side by side, one object "Pattern 2  grid of 3 × 2" | |
| 12 | Make the sphere bigger (its step in Objects) | All 6 grow | |
| 13 | Grid with both directions the same | "Pick two different directions for the grid."; Apply greyed | |
| 14 | How many 2000 | "That's more than 1000 copies…"; Apply greyed | |
| 15 | A box with a Pocket made from a sketch on its top; Pattern its last step in a row | The pocket repeats | |
| 16 | A cylinder with a Follow groove; Pattern its last step round a circle (line straight up through its middle), 4 | 4 grooves round it | |
| 17 | Build tab → Arrange → **Pattern** | Same tool | |
| 18 | Save, New, Open | Patterns come back and can still be changed | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
