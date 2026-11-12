# Test sheet: slice 5a (Export, Send to Slicer)

Time needed: about 15 minutes. You'll need a slicer installed (PrusaSlicer, Bambu
Studio, OrcaSlicer, Cura or similar) for checks 9 to 13.

## Get the build

Merge PR #15 (slice 4c) first if it isn't merged yet. Then open the pull request
**Slice 5a: Export and Send to Slicer (0.10.0)**, open its Windows build check, download
the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.10.0.

## Checks

### Export

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, a Cylinder and a Sphere. Click nothing (empty space), then **Export** in the toolbar | A save dialog titled "Export 3 shapes", type **3MF** | |
| 2 | Save it as `test.3mf` | The status bar says "Exported 3 shapes to ... (N triangles)" | |
| 3 | Open `test.3mf` in Windows 3D Viewer, 3D Builder or your slicer | The three shapes, where they were, smooth and the right size (box 20 mm) | |
| 4 | Select only the sphere, press **Ctrl+E**, choose type **STL**, save | Dialog title "Export 'Sphere 1'"; the STL holds just the sphere | |
| 5 | Export everything as **OBJ** | Opens in 3D Viewer / Blender as three objects | |
| 6 | Export again | The dialog opens in the same folder with the same type as last time | |
| 7 | Make something with Subtract, Split and Mirror, export it as 3MF, open in your slicer | No errors or "repair" warnings in the slicer | |
| 8 | Select a part inside a combined shape, Export | The whole combined shape is exported | |

### Send to Slicer

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | Click **Send to Slicer** | The first time: a list of slicers found on your PC, plus "The program Windows opens .3mf files with" and "Browse for a slicer program..." | |
| 10 | Pick your slicer | It opens with the shapes loaded, ready to slice | |
| 11 | Change a shape, click Send to Slicer again | No question this time; the slicer opens the updated shapes | |
| 12 | **File > Choose Slicer...**, pick "Browse for a slicer program..." and choose the slicer's .exe | Send to Slicer uses that program from now on | |
| 13 | Tell me which slicer you use and whether it was in the list | | |

## Send back

"All pass" or the failures, the answer to check 13, and `forge3d.log` if anything failed.
