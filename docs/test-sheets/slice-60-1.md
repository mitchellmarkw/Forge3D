# Test sheet: 0.69.1, straight lines for stamps and panel lines

Time needed: about 5 minutes.

## Get the build

Open the pull request **0.69.1: Shift for level, Ctrl for upright lines**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere; Panel Lines; click a point, then hold **Shift** and click higher up and to the side | The line runs level round the sphere from the first point (the height of the first point, not where you clicked) | |
| 2 | End Line; click a point, hold **Ctrl** and click off to one side and below | The line goes straight down the sphere as you see it | |
| 3 | Click without Shift or Ctrl | The line goes straight to where you clicked, as before | |
| 4 | On a box: Shift-click round a corner onto the next face | A level line round the corner | |
| 5 | Stamp: hold **Shift** while dragging a row up and across; then **Ctrl** | A level row; an upright row | |
