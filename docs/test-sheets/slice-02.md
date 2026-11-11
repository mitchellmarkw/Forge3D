# Test sheet: slice 2 (document model, undo and redo)

Time needed: about 15 minutes.

The buttons in the toolbar are **temporary test tools**. They exist only to exercise
the new undo system; the real Build tools (primitive library, gizmo, booleans) replace
them in slice 3.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-02-document-undo`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.
   This sheet is also in the zip, in the `test-sheets` folder.

## Checks

Mark each one Pass or Fail. For a Fail, add a sentence or a screenshot.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Start `Forge3D.exe` | Title "Untitled - Forge3D 0.2.0". An empty build plate. Status bar: "Empty plate - click Add Box or Add Cylinder". *Undo* and *Redo* are greyed out | |
| 2 | Click **Add Box** | A blue box appears in the middle of the plate. Title gains a `*`. The first toolbar button now reads **Undo Add Box** | |
| 3 | Click **Add Cylinder**, then **Add Box** again | An orange cylinder, then a green box, each placed to the right of the last with a gap; none overlap. Status bar: "3 bodies \| … triangles" | |
| 4 | Press **Ctrl+Z** | The green box disappears. *Redo* now reads **Redo Add Box** | |
| 5 | Press **Ctrl+Y** | The green box comes back, in the same place and the same colour | |
| 6 | Press **Ctrl+Z** twice more, then **Ctrl+Shift+Z** twice | Bodies vanish newest-first, then return in the same order | |
| 7 | Press **Ctrl+Z** until Undo is greyed out | The plate is empty and the `*` in the title is gone | |
| 8 | Press **Ctrl+Y** until Redo is greyed out | All three bodies are back | |
| 9 | Click **Make Last Taller** twice | The newest body grows taller each time. Undo reads **Undo Resize Box** (or Cylinder) | |
| 10 | Press **Ctrl+Z** twice | The body shrinks back one step at a time to its original height | |
| 11 | Click **Delete Last** | The newest body disappears. Undo reads **Undo Delete Box 2** (the body's name) | |
| 12 | Press **Ctrl+Z** | The body returns in the same place with the same colour | |
| 13 | Click **Clear Plate** | Every body disappears at once. Undo reads **Undo Clear Plate** | |
| 14 | Press **Ctrl+Z** once | **All** the bodies come back with a single undo | |
| 15 | Press **Ctrl+Z** (Redo becomes available), then click **Add Cylinder** | Redo is greyed out again: a new action clears the redo history | |
| 16 | Open the **Edit** menu | The Undo and Redo entries show the same labels as the toolbar buttons | |
| 17 | Add 8 or more bodies, then press **F** | The view fits all of them | |
| 18 | *File → New* | A "Discard the current model?" question. **Cancel** keeps everything; **Discard** empties the plate and greys out Undo | |
| 19 | Add a body, then close the window | The same Discard / Cancel question appears. **Cancel** keeps the app open | |
| 20 | Close it again and choose **Discard** | The app closes with no error | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` (*Help → Open log folder*). It now records how long startup takes,
  in lines starting with `Startup:`, and one `Command:` line per action. Please start
  the app **twice** and send the log from the second start too: comparing the two
  tells us whether Windows Defender is slowing the first launch.
