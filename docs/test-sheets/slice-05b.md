# Test sheet: slice 5b (Import with repair)

Time needed: about 20 minutes. Sample files are in `test-sheets\samples` next to this
sheet. Any STL / 3MF / OBJ of your own is welcome too.

## Get the build

Open the pull request **Slice 5b: Import with repair (0.11.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.11.0.

## Checks

### Importing

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Import** (toolbar) → `ring.obj` | A ring on the plate, named "ring", selected. Status: "Imported 1 shape (no repairs needed)" | |
| 2 | Import `ball-and-post.3mf` | Two shapes, "Ball" and "Post", side by side to the right of the ring, both selected. One Undo removes both | |
| 3 | Import `box-with-hole.stl` | A message: "Repaired: 1 hole filled." The box looks closed from every side | |
| 4 | Import `inside-out-box.stl` | A message: "Repaired: 1 inside-out piece turned the right way." It looks normal (not dark or see-through) | |
| 5 | Drag `ring.obj` from File Explorer onto the window | It's imported (named "ring 2") | |
| 6 | Select several files in the Import dialog at once | All of them come in | |
| 7 | Import one of your own STL files (a real print) | It appears at the right size; note how long it took if it has many triangles | |
| 8 | Drag a .f3d file onto the window | It opens (asks about unsaved changes first) | |

### Working with imported shapes

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | Select the ring | Properties: Shape "Mesh", "Imported: N triangles", Width / Depth / Height | |
| 10 | Change its Height, then drag a side handle (R) | It stretches like a box | |
| 11 | Move, turn, Mirror, Duplicate it | All work | |
| 12 | Put a cylinder through the box from check 3 and **Subtract** | A hole through the imported box | |
| 13 | **Split** an imported shape | It cuts like any other shape | |
| 14 | Export the result as 3MF and open it in your slicer | No repair warnings | |
| 15 | Save, close, reopen | The imported shapes come back, with their sizes | |

## Send back

"All pass" or the failures, plus how long check 7 took (and the triangle count, from the
status bar or the log), and `forge3d.log` if anything failed.
