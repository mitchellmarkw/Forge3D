# Test sheet: slice 15b, settings are remembered (0.24.1)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Settings are remembered (0.24.1)**, open its Windows build check,
download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.24.1.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Start Forge3D | The tour starts (this build keeps its settings in a new place, so it counts as a first run) | |
| 2 | **Finish** (or **Skip tour**), close Forge3D, start it again | No tour | |
| 3 | Switch to the Print tab, close, start again | It opens on the Print tab | |
| 4 | Add a box, **Send to Slicer**, choose your slicer. Close Forge3D, start it, add a box, Send to Slicer | The second time it goes straight to your slicer without asking | |
| 5 | Export a box as 3MF into a folder of your choice. Close, start, add a box, Export | The dialog opens in that folder with 3MF chosen | |
| 6 | Help → Take the Tour | Still runs on request | |

## Send back

"All pass" or the failures (with the number).
