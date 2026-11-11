# Test sheet: slice 3d (shape library)

Time needed: about 20 minutes.

New: **Add Shape** in the toolbar (and the new **Add** menu) offers Box, Cylinder,
Sphere, Cone, Torus, Tube and Wedge. It replaces the Add Box and Add Cylinder buttons.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03d-primitives`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.

## Checks

### Adding shapes

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Click **Add Shape** | A list: Box, Cylinder, a line, then Sphere, Cone, Torus, Tube, Wedge. Hovering an entry shows a short description | |
| 2 | Add one of each, in that order | Seven shapes in a row on the plate, none overlapping, all resting on the plate. Each is named after its type in Objects | |
| 3 | Open the **Add** menu in the menu bar | The same seven shapes | |
| 4 | Orbit around the torus and the tube | The torus is a doughnut lying flat with a clean hole; the tube is a pipe you can see through | |
| 5 | Orbit around the wedge | A ramp: tall at its left (-X) end, sloping down to the plate at the right | |

### Sizes in Properties

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | Select the sphere | Size shows one field: **Radius**. Change it to `15`: it grows and still sits on the plate | |
| 7 | Select the cone; set Top radius to `4` | Fields: **Bottom radius**, **Top radius** (was 0), **Height**. The tip is now cut off flat | |
| 8 | Set the cone's Bottom radius to `0` | It turns upside down: a point at the bottom, still touching the plate | |
| 9 | Select the torus; set Tube radius bigger than Ring radius | Fields: **Ring radius**, **Tube radius**. A message says the tube radius must be less than the ring radius; nothing changes | |
| 10 | Select the tube; set Inner radius to `9` | Fields: **Outer radius**, **Inner radius**, **Height**. The wall gets thinner | |
| 11 | Select the wedge | **Width**, **Depth**, **Height**, like a box | |
| 12 | Select the cylinder | Radius X, Radius Y, the **Round** checkbox, Height, as before | |
| 13 | Tick **Uniform scale**, select the tube, change Height to double | Every size doubles, hole included | |
| 14 | Ctrl+Z a few times | Each change undoes one step at a time | |

### Resize handles (press R)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 15 | Sphere: drag a side handle, then the top handle | It stays a ball either way and stays on the plate. Status bar says "Radius" or "Diameter" | |
| 16 | Cone: drag a side handle | It widens and keeps its taper (a cut-off top grows too) | |
| 17 | Torus: drag a side handle, then the top handle | Side: the whole ring gets bigger. Top: the ring gets thicker (and wider if it has to, to keep its hole) | |
| 18 | Tube: drag a side handle | Wider pipe, same wall thickness | |
| 19 | Wedge: drag the +X side handle | It stretches to the right; the tall end stays where it was | |

### Working with other features

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 20 | Put the sphere halfway into the tube's top (Align Shapes → **Align Face Center Z** with the tube picked first), then **Merge** | One shape; turning it with E turns about its middle | |
| 21 | Subtract a cone from a box | A cone-shaped dent or hole | |
| 22 | Turn a torus 90° around X, then **Sit on plate** | It stands on edge, touching the plate | |
| 23 | Save, close, reopen | Every shape comes back with its sizes | |
| 24 | Open a file saved with 0.5.x | Opens as before | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` if anything failed.
- Any shape you'd like next, or any size you'd expect to be able to change but can't.
