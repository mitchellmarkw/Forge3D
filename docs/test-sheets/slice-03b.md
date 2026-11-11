# Test sheet: slice 3b (gizmo: drag to move, rotate and resize)

Time needed: about 20 minutes.

New in this slice: drag bodies with the mouse. Three tools sit in the toolbar (also in the
Edit menu): **Move (W)**, **Rotate (E)** and **Resize (R)**. **Snap** is on by default:
moves go in 1 mm steps, turns in 15° steps, sizes to whole millimetres. Hold **Alt**
while dragging to move freely.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03b-gizmo`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.
3. Click **Add Box**, **Add Cylinder** and **Add Box** to have something to work with.

## Checks

Mark each one Pass or Fail. For a Fail, add a sentence or a screenshot.

### Sliding on the plate

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Left-drag a body that is **not** selected | It becomes selected and slides across the plate under the mouse, staying on the plate. The status bar shows "Move X … mm Y … mm" | |
| 2 | Release, then look at Undo | One step: **Undo Move**. Ctrl+Z puts the body back in one go | |
| 3 | Start dragging a body, then press **Esc** before releasing | The body jumps back to where it started; the status bar says "Drag cancelled"; nothing new to undo | |
| 4 | Left-click (no drag) a body | It is selected but does not move | |

### Move tool (W)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 5 | Select a body and press **W** | Three arrows from the body's centre: red (X), green (Y), blue (Z, up). **Move** is highlighted in the toolbar | |
| 6 | Hover over an arrow | It turns yellow and the pointer becomes a hand | |
| 7 | Drag the red arrow | The body moves only along X, in 1 mm steps. The arrow stays yellow and the others fade while dragging | |
| 8 | Drag the blue arrow up | The body lifts off the plate. **Sit on plate** in Properties puts it back down | |
| 9 | Drag an arrow while holding **Alt** | Movement is smooth; X / Y / Z in Properties show values like 12.37 | |
| 10 | Turn **Snap** off in the toolbar and drag again | Smooth without Alt. Turn Snap back on afterwards | |

### Rotate tool (E)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 11 | Select a body and press **E** | Three rings around it: red (around X), green (around Y), blue (around Z) | |
| 12 | Drag the blue ring around the body | It turns on the plate in 15° steps; the status bar shows the angle; **Around Z** in Properties follows | |
| 13 | Drag the red ring | The body tips forward or back; the part of the ring you grabbed follows the mouse | |
| 14 | Keep dragging a ring all the way round (past 180°) | The turn keeps going smoothly with no jump | |
| 15 | Ctrl+Z | The whole turn undoes in one step | |

### Resize tool (R)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 16 | Select a box and press **R** | Six small square handles, one just outside each face, coloured by axis | |
| 17 | Drag the top (blue) handle up | The box gets taller; the bottom stays on the plate. Status shows "Resize Height … mm"; Height in Properties follows in whole mm | |
| 18 | Drag the right-hand (red) handle outward | The box gets wider on that side only; the opposite face does not move | |
| 19 | Drag a handle inward a long way | The size stops at 1 mm; it never flips inside out | |
| 20 | Select a cylinder; drag a red or green handle | The **Radius** changes; the cylinder stays centred | |
| 21 | Tilt a box with the Rotate tool, then resize it with R | The handles follow the box's tilted faces, and dragging one moves that face | |
| 22 | Ctrl+Z after each resize | Each resize undoes in one step, back to the exact earlier size and position | |

### General

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 23 | With a handle visible, click on the handle without dragging | The selection does not change | |
| 24 | Click a name field in Properties and type the letters w, e and r | They type normally and do not switch tools | |
| 25 | Zoom far out and close in (mouse wheel) | The gizmo stays the same size on screen | |
| 26 | Save, close, reopen | Everything you dragged comes back exactly | |

## Send back

- The results table (or "all pass" plus any failures).
- Anything that felt awkward: handle sizes, drag speed, which way rings turn, snap steps.
  This is the part of the app you will use most, so feel matters as much as correctness.
- `forge3d.log` if anything failed.
