# Test sheet: slice 40, Smooth, Extrude Down, looks (0.49.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 40: Smooth, Extrude Down, Solid / See-Through / Wireframe
(0.49.0)**, open its Windows build check, download the `Forge3D-windows-x64-…` artifact,
unzip and run `Forge3D.exe`.

## Checks

### Looks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box and a Cylinder, turn the view round | **Solid** now: you can't see into the shapes; near faces hide far ones from every side. (Before, shapes looked hollow: that was a drawing fault, now fixed) | |
| 2 | View tab: a **Look** group with **Solid**, **See-Through** and **Wireframe**; click **See-Through** | Both shapes half clear: you can see their far edges, and the grid through them. The View menu has the same three, with See-Through ticked | |
| 3 | Click **Wireframe** | Every shape drawn as the edges of its triangles, in its colour; a selected one in cyan | |
| 4 | In Wireframe or See-Through: click a shape, drag it, use the handles | Picking and dragging work as in Solid | |
| 5 | With See-Through on, close Forge3D and start it again | Still See-Through. Back to **Solid** for the rest | |

### Smooth

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | Select the box; Sculpt tab → **Whole Shape** → **Smooth** | After a moment its edges and corners are rounded. Objects shows a "Smoothed" step, strength 3 | |
| 7 | In Properties set **Strength** to 10 | Much rounder, nearly a pebble; about the same size. Undo: back to 3 | |
| 8 | Click **Remove the smoothing** in Properties | The sharp box again | |
| 9 | Smooth an imported or sculpted mesh (yours, the bigger the better) | Smoothed; note how long it takes | |

### Extrude Down

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 10 | Add a Sphere and set its **Z** to 10. Design tab → Place → **Extrude Down** | The ball sits on a round column down to the plate (like a capsule) | |
| 11 | Merge a tall thin box with a wide flat box on top (a T), set it so it stands on the plate, and Extrude Down | The space under both arms of the T is filled: a solid block | |
| 12 | Extrude Down a shape sitting flat on the plate (a box) | No change | |
| 13 | Move a shape so part of it is below the plate, then Extrude Down | A message says to move it up first | |
| 14 | Extrude Down one of your figures (arms out) | Everything below the arms and body is filled to the plate. Note how long it takes | |
| 15 | Save, close and reopen the file with a Smooth and an Extrude Down step | Both as they were | |
