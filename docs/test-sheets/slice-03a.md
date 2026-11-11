# Test sheet: slice 3a (select, edit, save and open)

Time needed: about 20 minutes.

New in this slice: click to select, an **Objects** list on the left, a **Properties**
panel on the right, and Save / Open with `.f3d` files. The Add Box / Add Cylinder /
Clear Plate buttons are still temporary; the real primitive library comes in 3c.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03a-select-edit-save`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.

## Checks

Mark each one Pass or Fail. For a Fail, add a sentence or a screenshot.

### Selecting

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Start the app | Title "Untitled - Forge3D 0.3.0". Objects panel on the left (empty), Properties panel on the right saying "Click a body…" | |
| 2 | Click **Add Box**, then **Add Cylinder** | Each new body appears **already selected**: it glows cyan, is highlighted in the Objects list, and its values show in Properties | |
| 3 | Left-click the box in the view | The box glows; the cylinder returns to its normal colour. The list and Properties follow | |
| 4 | Left-click empty space (the plate or background) | Nothing is selected; Properties shows the hint again | |
| 5 | Click a name in the Objects list | That body is selected in the view | |
| 6 | Select a body and press **Esc** | Selection clears | |
| 7 | Right-drag to orbit while a body is selected | Orbiting does not change the selection | |

### Editing in Properties

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 8 | Select the box. In **Width (X)** type `50` and press **Enter** | The box gets wider straight away. Undo reads **Undo Resize Box** | |
| 9 | Click the up arrow on **Height (Z)** three times | The box grows a little with each click | |
| 10 | Press **Ctrl+Z** four times | The height steps back three times, then the width returns to 30 | |
| 11 | Set **X** to `-40` and **Rotation** to `45`, pressing Enter after each | The box moves left, then turns 45°. These are two separate undo steps ("Undo Move") | |
| 12 | Set **Lift (Z)** to `10` | The box floats 10 mm above the plate | |
| 13 | Change **Name** to `Lid` and press Enter | The Objects list shows "Lid". Undo reads **Undo Rename** | |
| 14 | Select the cylinder; check Properties | It shows **Radius** and **Height** instead of width/depth/height | |
| 15 | Select a body and press **Delete** (or the Delete button) | It disappears; Ctrl+Z brings it back in the same place | |
| 16 | Try to set a size to `0` | The field will not go below 0.01 mm | |

### Saving and opening

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 17 | Press **Ctrl+S** | A Save dialog opening in your Documents folder, suggesting "Untitled.f3d". Save as `test1` | |
| 18 | Look at the title | "test1.f3d - Forge3D 0.3.0" with no `*`. Status bar says "Saved …" briefly | |
| 19 | Move a body, then look at the title | A `*` appears. Press Ctrl+S: it saves straight away with no dialog, and the `*` goes | |
| 20 | Make a change, then *File → New* | "Save changes to test1.f3d before closing it?" with **Save / Discard / Cancel**. Choose **Discard** | |
| 21 | *File → Open* (Ctrl+O) and pick `test1.f3d` | The dialog opens in the folder you last used. Every body comes back with the same names, sizes, positions, rotations and colours. Undo is greyed out | |
| 22 | *File → Save As* and save as `copy of test1` in a folder whose name has spaces or accents (e.g. `Größe Test`) | Saves fine; the title shows the new name | |
| 23 | Make a change and close the window | The Save / Discard / Cancel question. **Cancel** keeps the app open; **Save** saves and closes | |
| 24 | Drag `test1.f3d` onto `Forge3D.exe` in Explorer | The app starts with that model open | |
| 25 | Make a copy of `test1.f3d`, rename it to `broken.zip`, open it with Windows' zip viewer, and look inside | One file, `document.json`, readable in Notepad | |
| 26 | Create an empty text file, rename it `junk.f3d`, and open it in Forge3D | A clear message: "This file is damaged or is not a Forge3D file." The current model is untouched | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` (*Help → Open log folder*). It now records `Undo:`, `Redo:`, `Saved` and
  `Opened` lines too.
- If anything looks off in the view, a screenshot.
