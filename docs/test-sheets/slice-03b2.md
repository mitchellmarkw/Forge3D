# Test sheet: uniform scale and elliptical cylinders (0.4.1)

Time needed: about 15 minutes.

New: a **Uniform** toggle (toolbar, Edit menu, key **U**, and a checkbox in Properties →
Size) that makes resizing keep proportions; and cylinders with separate **Radius X** and
**Radius Y**, controlled by a **Round** checkbox.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03b2-scale-ellipse`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.

## Checks

### Uniform scale

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a box (30 x 20 x 15). Press **R**, then **U** (or click **Uniform**) | Uniform is highlighted in the toolbar, and **Uniform scale (keep proportions)** is ticked in Properties | |
| 2 | Drag the top handle up | All three sizes grow together; the status bar shows "(uniform x1.50)" or similar; the box stays on the plate | |
| 3 | Drag a side (red) handle outward | All three sizes grow; the opposite side stays where it was; the box stays on the plate | |
| 4 | Drag a handle far inward | The box shrinks evenly and stops before any size reaches 0 | |
| 5 | In Properties, set **Width** to `60` and press Enter | Depth and Height change by the same factor (a 30 x 20 x 15 box becomes 60 x 40 x 30) | |
| 6 | Untick **Uniform scale** in Properties | The toolbar **Uniform** button turns off too | |
| 7 | Set **Width** to `80` | Only the width changes | |
| 8 | Ctrl+Z through the steps above | Each change undoes in one step | |

### Elliptical cylinders

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | Add a cylinder and look at Properties | **Radius X** and **Radius Y** (Y greyed out), a ticked **Round** box, and Height | |
| 10 | With Round ticked, press **R** and drag the red side handle | The cylinder stays round; both radii change together | |
| 11 | Untick **Round**. Drag the red handle outward | Only **Radius X** grows: the cylinder becomes an oval, growing evenly either side of its centre. Status reads "Resize Radius X …" | |
| 12 | Drag the green handle inward | Only **Radius Y** shrinks | |
| 13 | Type a value in **Radius Y** and press Enter | Only that radius changes | |
| 14 | Look at the oval closely, orbiting around it | A smooth, even outline with no flat spots or bulges | |
| 15 | Tick **Round** again | The cylinder becomes round, using the Radius X value. Ctrl+Z makes it oval again | |
| 16 | Select an oval cylinder, then a round one, then the oval again | Round is unticked for the oval one and ticked for the round one | |
| 17 | Turn **Uniform** on and drag any handle on the oval | It scales evenly and stays oval (the same shape, bigger or smaller) | |
| 18 | Rotate the oval with **E**, then **R**-resize it | The handles follow the tilted shape; dragging still changes the right radius | |

### Files

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 19 | Save a file with an oval cylinder, close, and reopen it | Both radii come back exactly | |
| 20 | Open `test2.f3d` (saved with 0.3.1) | Its cylinders open round and unchanged | |
| 21 | Select a box after a cylinder | Properties shows Width / Depth / Height with no blank gap below them | |

## Send back

- The results table (or "all pass" plus any failures).
- How the oval resizing feels: should a side handle on an oval grow both sides evenly (as
  now), or keep the opposite side fixed like a box?
