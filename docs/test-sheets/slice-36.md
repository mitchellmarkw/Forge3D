# Test sheet: slice 36, Sketch on the section's cut (0.45.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 36: Sketch on the section's cut (0.45.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 40 × 40 × 30. Design → the arrow beside **Sketch** | "The section view's cut" at the bottom, greyed out | |
| 2 | View → **Section View** (cut across X, in the middle). Select the box, Design → **Sketch**, then click the dark cut face | The sketcher opens: "Sketch on the section view's cut through 'Box 1'", with the box's cut (a 40 × 30 rectangle) in grey dashes; **The other way** is ticked | |
| 3 | Draw a circle inside the rectangle. **Make: Pocket**, Depth 10, **Finish** | Back in the 3D view (still cut): a round hole 10 deep going into the half you can see, starting at the cut | |
| 4 | Close Section View | The box looks whole from outside: the hole is a hidden chamber inside | |
| 5 | Section View again, **Show the other side** | The other half: no hole in it (the pocket went one way) | |
| 6 | Untick Show the other side. Slide the cut to another place. Select the box, **Sketch ▾ → The section view's cut** | The sketcher opens at the new place; the dashed outline now also shows the chamber if the cut passes through it | |
| 7 | Draw a small rectangle, Pocket, tick **Both ways**, Depth 20, Finish | A slot 10 each side of the cut | |
| 8 | Double-click that step in Objects | Its sketch opens again to change; Cancel | |
| 9 | Change Cut across to **Bottom to top (Z)**, select the box, Sketch, click the cut face | The sketch is on the level cut, seen from above | |
| 10 | Select the box, Sketch, and click an outside face that still shows (not the cut) | The sketch goes on that face, as it always did | |
| 11 | With nothing selected: Sketch ▾ → The section view's cut; draw a circle; Pad 5; Finish | A disc standing on edge at the cut's plane | |
| 12 | Undo a few times; Save, close, open | Undo steps back cleanly; the model opens as saved | |

## Send back

"All pass" or the failures (with the number).
