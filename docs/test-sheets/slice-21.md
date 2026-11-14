# Test sheet: slice 21, sculpting kept in the history (0.30.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 21: Sculpting kept in the history (0.30.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, select it, Sculpt tab → **Sculpt**. Drag a stroke across the top, then **Done** | The stroke stays. In Objects the shape is now **Sculpted 1**, "1 stroke"; open it to see **1. Box** and **2. Sculpted 1** | |
| 2 | **Edit → Undo**, then **Redo** | Undo gives back the plain box; Redo brings the stroke back straight away | |
| 3 | Click **1. Box** in Objects and change its Width (e.g. 20 → 40) | The box gets longer and the stroke is made again on it, in the same place (may take a moment) | |
| 4 | Double-click **Sculpted 1** | The Sculpt panel opens titled "Change the sculpting of …", with the stroke already there; Detail can't be changed | |
| 5 | Add two strokes, click **Undo stroke** once, then **Done** | One new stroke kept; Objects shows "2 strokes" | |
| 6 | Double-click it again, add a stroke, press **Esc** | The shape is as it was before (2 strokes) | |
| 7 | Sculpt with **Mirror X** on, and with **Grab**, then Done; change the box's Depth | The mirrored stroke and the pull are made again on the new box | |
| 8 | Rotate and move the sculpted shape, then double-click it and add a stroke | The brush ring sits on the surface under the mouse and the stroke goes where you drag | |
| 9 | Save, close, open the file | The sculpted shape is the same; double-click still opens it with its strokes | |
| 10 | Select the sculpted shape, Properties → **Remove the sculpting** | The plain box comes back where it was | |
| 11 | Sculpt a Cylinder or Sphere at Detail **High**, about 20 strokes, Done | Done takes no longer than before; Undo/Redo are quick | |
| 12 | Open a file sculpted with 0.29 (a mesh shape) | It opens as before | |

## Send back

"All pass" or the failures (with the number).
