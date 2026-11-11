# Test sheet: slice 3d2 (oval shapes, pyramid)

Time needed: about 20 minutes.

New: a **Pyramid** in Add Shape, and oval radii for the sphere, cone, torus and tube
(the **Round** checkbox, as on the cylinder).

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03d2-ovals-pyramid`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.

## Checks

### Pyramid

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Add Shape → Pyramid** | A square pyramid with a point. Properties: Base width (X), Base depth (Y), Top width (X), Top depth (Y), Height | |
| 2 | Set Base depth to `30` | The base gets longer along Y | |
| 3 | Set Top depth to `30` (Top width still 0) | A roof shape: a ridge along Y | |
| 4 | Set Top width to `8` | A flat-topped pyramid | |
| 5 | Set Top width `30` and Top depth `40` (bigger than the base) | Upside down: wider at the top, still standing on the plate | |
| 6 | Press **R** and drag the +X side handle | It stretches along X; the -X side stays put | |

### Round checkbox

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 7 | Add a Sphere and select it | Radius X, Radius Y, Radius Z (up), and **Round (X, Y and Z radii equal)** ticked. Y and Z are greyed out | |
| 8 | Change Radius X to `15` | It stays a ball, all three radii 15 | |
| 9 | Untick Round, set Radius Z to `6` | A squashed ball, still on the plate | |
| 10 | Set Radius Y to `25` | An egg lying along Y | |
| 11 | Tick Round again | Back to a ball, using Radius X | |
| 12 | Ctrl+Z | The egg comes back, and **Round** unticks | |

### Oval cone, tube, torus

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 13 | Add a Cone, untick Round, set Bottom radius Y to `5` | An oval base, still a point on top | |
| 14 | Set Top radius X to `4` | Top radius Y becomes 4 too (an end is a point or has both radii): the tip is cut off flat | |
| 15 | Set Top radius Y to `2` | An oval flat top. Set Top radius X to `0`: Y goes to 0 too and the point is back | |
| 16 | Add a Tube, untick Round, set Outer radius Y to `15`, Inner radius Y to `12` | An oval pipe | |
| 17 | Set Inner radius X bigger than Outer radius X | A message; nothing changes | |
| 18 | Add a Torus, untick Round, set Ring radius X to `20` | An oval ring (like a running track) | |
| 19 | Set Tube radius Z (up) to `6` | The ring gets taller but no wider | |
| 20 | Set Tube radius to `9` | A message: the tube is too big for this oval ring; nothing changes | |

### Resize handles (press R)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 21 | With Round ticked, drag a side handle of a sphere, cone, tube and torus | Each stays round; the status bar says Radius / Outer radius | |
| 22 | Untick Round and drag the Y side handle of each | Only that direction stretches; the status bar adds " Y" | |
| 23 | Sphere with Round unticked: drag the top handle | Only the height changes | |

### Files

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 24 | Save with an egg, an oval cone, oval torus, oval tube and pyramids; close; reopen | All come back the same | |
| 25 | Open a file saved with 0.6.0 that has a sphere, cone, torus and tube | They open, round, as before | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` if anything failed, or if an oval torus took more than a second to appear.
