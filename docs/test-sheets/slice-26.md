# Test sheet: slice 26, Threads (0.35.0 Beta)

Time needed: about 15 minutes (plus printing, if you like).

## Get the build

Open the pull request **Slice 26: Threads (0.35.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Cylinder, radius 3, height 15. Select it, Design tab → **Thread** | The Thread panel; the status bar says to click the round face | |
| 2 | Click the cylinder's top | "That face isn't round…" | |
| 3 | Click the cylinder's side | Its edges outlined red; "A peg, Ø 6.00 mm…"; **Thread size** (its label above it) set to **M6 (pitch 1 mm)**; a preview of the thread | |
| 4 | Choose M5, then **Other size** and type 6.5 / 0.9 | The preview follows; typing a standard size puts the list back on it | |
| 5 | Tick **Left-hand** | The thread turns the other way in the preview | |
| 6 | Back to M6, right-hand, **Apply** | A **Threaded** step, "M6 thread"; the peg isn't any taller or shorter, and doesn't cut into the plate | |
| 7 | Double-click the step, change Fit to 0.3, Apply | The thread changes; Undo puts it back | |
| 8 | Click the Cylinder inside the step and make it 25 tall | The thread runs the new length | |
| 9 | Make a Box 20 × 20 × 10 with a hole through it (Hole tool, or subtract a cylinder of radius 2.5). **Thread** → click the inside of the hole | "A hole, Ø 5.00 mm…", **M6** suggested; Apply cuts an inside thread | |
| 10 | Thread an imported STL, or a sculpted shape | A message that threads go on solid shapes | |
| 11 | Save, close, open | The same threads | |
| 12 | (If you can) print the M6 peg and the M6 nut block | They screw together; if too tight or loose, try Fit 0.3 or 0.1 | |

## Send back

"All pass" or the failures (with the number).
