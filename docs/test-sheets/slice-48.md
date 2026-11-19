# Test sheet: slice 48, Split by Mask (0.57.0 Beta)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 48: Split by Mask (0.57.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Cylinder. Select it, Design → Change → **Split by Mask** | The panel: Brush size, Keep (label above the drop-down), Undo Stroke, Clear, Apply | |
| 2 | Move over the shape, then drag across it | A ring shows the brush; dragging paints the surface darker, evenly, however fast you drag | |
| 3 | **[** and **]** | The brush (and its ring) gets smaller and bigger | |
| 4 | Hold **Ctrl** and drag over some paint | It's rubbed out | |
| 5 | **Undo Stroke**, Ctrl+Z, then **Clear** | The last stroke goes; then all of it | |
| 6 | Paint a band right round the cylinder's middle (turn the view with the right button to reach the back). Keep: Both pieces. **Apply** | Two shapes: "Cylinder 1 (masked)" (the band) and "Cylinder 1 (rest)". One Undo puts the cylinder back | |
| 7 | Move the band piece aside | The cut ends are closed (no holes), slightly curved where the paint's edge wasn't straight | |
| 8 | Paint over a corner of a Box and split it | The corner comes off along a smooth curve, not in straight steps | |
| 9 | Export both pieces of 6 or 8 to your slicer | Both are solid and print-ready (no repair needed); they fit back together | |
| 10 | Double-click the step ("2. … (masked)") in Objects | The whole part is shown with the paint; the other piece is hidden. Paint some more, Apply: both pieces change | |
| 11 | **Remove the cut** on one piece (Properties) | That piece is the whole shape again | |
| 12 | Split one of your figure meshes by mask: an arm off a torso, say, painting round the shoulder | It cuts along the paint; both pieces closed. Note how long Apply takes | |
| 13 | Keep: **Just the painted piece**, then (Undo) **Just the rest** | Only that piece is made | |
| 14 | Before painting, look at Apply; then paint over the whole shape and Apply | Apply is greyed out until something is painted; with all of it painted it says so and stays open | |
| 15 | Save, close and open the file | The split shapes are there and can still be changed | |
