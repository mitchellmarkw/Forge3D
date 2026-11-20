# Test sheet: slice 56, Rescale Design (0.65.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 56: Rescale Design (0.65.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box and a Sphere; select the sphere | Properties has **Joint part (keeps its size when rescaled)**, unticked | |
| 2 | Tick it; deselect; **Tools → Rescale Design…** | Rescale (label on top of its drop-down): Everything on the plate (greyed: nothing selected); How: From one scale to another, 1:12 → 1:18; "Keep joint sizes (1 joint found)"; the summary × 0.667 | |
| 3 | **Rescale** | The box shrinks to 13.33 mm and moves in; the sphere keeps R10 and moves in; both stay on the plate. One Undo puts it all back | |
| 4 | Again with Keep joint sizes unticked | The sphere shrinks too | |
| 5 | How: **To a new height**, type a height; then **By a percentage** | The summary follows; Rescale gives that height or size | |
| 6 | Select only the box, Rescale Design | Rescale: The selected shapes; only the box changes | |
| 7 | A cylinder with a Quick Socket ball socket (and the matching ball it can add). Rescale 1:12 → 1:18, Keep joint sizes ticked | The cylinder shrinks, the socket stays its size (history and double-click show the same size), in the same spot on the face; the ball stays its size | |
| 8 | A shape with a Quick Hinge (and its pieces), the same | The hinge, its tab and pin keep their size | |
| 9 | A Shell-hollowed shape, **Keep wall thickness** ticked, shrink it | The walls stay as thick (Section View) | |
| 10 | Split to Fit with pegs, then Rescale | The pegs and their holes keep their size | |
| 11 | Save, close, open; select the sphere from check 2 | Joint part is still ticked | |
