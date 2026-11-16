# Test sheet: slice 35, Backup settings, section clicks, measurement labels (0.44.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 35: Backup settings, section clicks, measurement labels
(0.44.0)**, open its Windows build check, download the `Forge3D-windows-x64-…`
artifact, unzip and run `Forge3D.exe`.

## Checks

### Backup copies

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Edit → Backup Copies…** (at the bottom of the menu) | A tick box "Keep a backup copy of unsaved work" and "Make one every: 2 minutes" | |
| 2 | Try to set 1 minute and 61 minutes | It only goes from 2 to 60 | |
| 3 | Untick the box | The minutes grey out. OK: the status bar says backup copies are off | |
| 4 | Change a model, wait 3 minutes, End task in Task Manager, start again | No recovery question (backups were off) | |
| 5 | Backup Copies: tick it again, 2 minutes, OK. Change a model, wait a little over 2 minutes, End task, start again | The recovery question, as before | |
| 6 | Restart Forge3D and open Backup Copies | Your settings are remembered | |

### Section view and clicks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 7 | Two boxes, one behind the other as you look at them. **Section View**, and slide the cut until the front box is completely hidden | Only the back box shows | |
| 8 | Click where the front box used to be, on the back box | The back box is selected (before, the hidden front box was) | |
| 9 | Slide the cut to halfway through a hollow shape (a shelled box) and click its inside | That shape is selected; dragging moves it | |
| 10 | Close Section View and click the front box | It's picked as usual | |

### Measurements in the view

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 11 | **Measure**; click a face on one shape and a face on another | The distance written on the green line in the view, the same number as in the panel | |
| 12 | Measure between two holes (or two cylinders) | Centre to centre written on the blue line too | |
| 13 | **Keep on Screen** | The picks clear; the line and number stay. Measure something else: both show | |
| 14 | **Done** | The kept ones stay; orbit and zoom: they stay on their shapes | |
| 15 | Move one of the shapes | The kept measurements go, with a message why | |
| 16 | Keep two more, then Measure → **Remove Kept** | They're gone | |

## Send back

"All pass" or the failures (with the number).
