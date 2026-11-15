# Test sheet: slice 22, mask, Crease and Snake hook (0.31.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 22: Mask, Crease and Snake hook (0.31.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, select it, Sculpt tab → **Sculpt**. Open the Brush list | Crease, Snake hook and Mask at the end; Clear mask and Invert mask buttons under the brush description | |
| 2 | Brush **Mask**: paint a stripe across the top | The stripe turns dark | |
| 3 | Brush **Draw**: drag across the stripe | The surface rises on both sides of the stripe but not on it | |
| 4 | **Invert mask**, then Draw across again | Now only the stripe rises; the rest is dark and stays put | |
| 5 | Mask brush with **Ctrl** held over part of the dark area | That part is unmasked (light again) | |
| 6 | **Undo stroke** twice | The unmasking, then the last Draw stroke, are taken back | |
| 7 | **Clear mask** | No dark areas left | |
| 8 | Brush **Crease**: drag a line; then with **Ctrl** | A sharp, narrow groove; with Ctrl a sharp ridge | |
| 9 | Brush **Snake hook**: press on the top and drag away from the shape | A horn is pulled out after the mouse, smooth along its length | |
| 10 | Tick **Mirror X**, Snake hook near one side | Two horns, one each side | |
| 11 | **Done**, then double-click the Sculpted step | The strokes are all there, and any mask left on is shown dark again | |
| 12 | After Done, the shape on the plate | No dark mask showing | |
| 13 | Click the Box inside the step and change its Width | The horns, creases and masked strokes are made again on the new box | |
| 14 | Save, close, open | The same shape | |

## Send back

"All pass" or the failures (with the number).
