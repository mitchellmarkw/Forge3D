# Test sheet: slice 34, Backup copies and recovery (0.43.0 Beta)

Time needed: about 10 minutes (most of it waiting).

## Get the build

Open the pull request **Slice 34: Backup copies and recovery (0.43.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Open one of your saved models. Add a shape to it (don't save). Wait a little over 2 minutes | Nothing to see: no pause, no message | |
| 2 | End Forge3D the hard way: Task Manager (Ctrl+Shift+Esc) → Forge3D → **End task** | It's gone | |
| 3 | Start Forge3D | "Forge3D didn't close properly last time. It kept a backup copy of *your file's name*, made *time*" with **Recover**, **Throw It Away**, **Ask Me Next Time** | |
| 4 | **Ask Me Next Time**; close Forge3D; start it again | The same question | |
| 5 | **Recover** | The model with the added shape; the title shows your file's name with a * (not saved yet) | |
| 6 | Look at your file in Explorer (its date), then **Save** in Forge3D | The file wasn't touched until you saved; Save writes it without asking for a name | |
| 7 | Close Forge3D normally and start it again | No question | |
| 8 | New model (never saved): add shapes, wait 2 minutes, End task, start again | The question says "a model that was never saved"; Recover brings it back as Untitled | |
| 9 | Do the same, but choose **Throw It Away**; start again | No question | |
| 10 | Make a change, wait 2 minutes, then **Save** and End task; start again | No question (it was saved) | |

## Send back

"All pass" or the failures (with the number).
