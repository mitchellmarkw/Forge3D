# Test sheet: slice 32, Recent files, plate size, the ribbon (0.41.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 32: Recent files, plate size, ribbon (0.41.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

### Open Recent

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Open two or three of your models, one after another; save a new one. **File → Open Recent** | A submenu listing them, newest first, numbered; pointing at one shows its folder | |
| 2 | Pick one from the list | It opens (asking about unsaved changes first, if there are any) and moves to the top of the list | |
| 3 | Close Forge3D and start it again; File → Open Recent | The list is still there | |
| 4 | Rename one of those files in Explorer, then pick it from the list | A message that it isn't there any more; it's gone from the list | |
| 5 | **Clear This List** | The submenu says no files yet | |

### Build plate size

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | **Edit → Build Plate Size…** (also in the View menu and on the View tab) | A window: **Printer bed:** with its label above the list, Across (X), Front to back (Y) | |
| 7 | Choose **270 × 270 mm**, OK | The grid is 270 × 270 with a darker line round its edge | |
| 8 | Open it again, type X 300 and Y 150 | The list changes to "Another size"; OK gives a long, narrow plate | |
| 9 | Close and restart Forge3D | The plate is still the size you set | |
| 10 | Set it to your own printer's size | | |

### The ribbon

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 11 | Look at the tabs | Build, Design, Sculpt, View, Print/Save | |
| 12 | **Build** | File, Add, Handles, Remove, then Check and Undo | |
| 13 | **Design** | Combine, Arrange, Sketch, Features, Change, Place, then Check and Undo | |
| 14 | **Sculpt** | Brushes, Triangles, then Check and Undo | |
| 15 | **View** | Workspace (Orbit, Pan, Zoom, Fit, Reset View), Objects (Section View), Plate (Plate Size), then Check and Undo | |
| 16 | **Print/Save** | Out (Save, Export, Send to Slicer), then Check and Undo | |
| 17 | With a couple of shapes: Design → Merge / Subtract, Align, Lay on Face | They work as they did from their old places | |
| 18 | View → **Orbit**, then drag in the view with the left button | The button stays pressed; the view turns; shapes aren't selected or moved | |
| 19 | Click **Pan**, drag; click **Zoom**, drag up and down | Orbit pops up as Pan goes down; the view slides; then zooms (up: closer) | |
| 20 | While Zoom is on: right-drag, middle-drag, wheel | They still turn, slide and zoom as always | |
| 21 | Click **Zoom** again; click and drag a shape | Zoom is off; the left button selects and moves again | |
| 22 | **Fit** and **Reset View** | Like F and Home; they don't stay pressed | |
| 23 | **Section View** on the View tab | The panel opens and the button shows pressed; clicking again (or closing the panel) turns it off | |
| 24 | Help → Take the Tour | The steps point at the right buttons on the new tabs | |

## Send back

"All pass" or the failures (with the number).
