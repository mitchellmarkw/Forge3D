# Test sheet: slice 5c (Mesh engine)

Time needed: about 15 minutes. You need the big organic model that froze before (about
1.5 million triangles). Sample files are in `test-sheets\samples` next to this sheet.

## Get the build

Open the pull request **Slice 5c: Mesh engine (0.12.0)**, open its Windows build check,
download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.12.0.

## Checks

### The case that froze

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Import your 1.5M-triangle model | It comes in as before (note the time) | |
| 2 | Add a Box, move it so it overlaps part of the model | | |
| 3 | Select both, click **Subtract** | A wait cursor for a few seconds (no question first), then the model with a box-shaped bite out of it. Note the time | |
| 4 | Orbit around the cut | Clean, closed cut faces; no holes or stray triangles | |
| 5 | Undo, then Redo | Undo brings back the model and box at once; Redo brings the cut back in about a second or less | |
| 6 | Select the box inside the result (click it in the result, as in 4b) and drag it to a new spot | When you let go, the cut moves with it in about a second | |
| 7 | Export the result as 3MF and open it in your slicer | No repair warnings. Note how long the export took | |
| 8 | Save, close, reopen | The cut model comes back the same | |

### The other tools on a big mesh

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | **Split** the big model through the middle, keep both | Two halves within a few seconds, each closed on the cut side | |
| 10 | **Mirror** one half | A mirrored copy, quickly | |
| 11 | Select a half and a cylinder, **Merge** | One shape | |
| 12 | Select the model and a sphere overlapping it, **Intersect** | Only the overlap is left | |
| 13 | **Group** the model with a box, then resize the group with a handle | Both resize together | |
| 14 | Subtract or Split so that nothing would be left (e.g. a box that covers the whole model) | The same "nothing left" message as for ordinary shapes; nothing changes | |

### Nothing else changed

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 15 | Repeat checks 1–4 and 12–13 from the slice 5b sheet with the sample files | Same as before | |
| 16 | Make a Merge / Subtract with only ordinary shapes (no imports) | Works as before; edges still look sharp and smooth (not triangulated) | |

## Send back

"All pass" or the failures, plus the times for checks 1, 3, 7 and 9, and `forge3d.log`
if anything failed.
