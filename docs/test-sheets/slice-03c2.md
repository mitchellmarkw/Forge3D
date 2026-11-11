# Test sheet: slice 3c2 (turning centre, Align Shapes)

Time needed: about 20 minutes.

New: combined shapes turn about the middle of what you can see, and **Align Shapes**
lines one shape up against another.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest green run for the
   branch `slice-03c2-pivot-align`.
2. Download the artifact `Forge3D-windows-x64-…`, unzip it, and run `Forge3D.exe`.

## Checks

### Turning a combined shape

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a box (20 × 20 × 20) and a cylinder. Give the cylinder Radius `5`, Height `40`, and put it at the box's X and Y, so it pokes out of the top | The cylinder stands twice as tall as the box | |
| 2 | Click the box, Ctrl+click the cylinder, **Subtract** | A box with a round hole | |
| 3 | Select the result and press **E** | The rings sit around the middle of the box (Z = 10), not halfway up where the cylinder was (Z = 20) | |
| 4 | Drag the red ring a quarter turn | The box turns in place: it does not lift off or sink into the plate | |
| 5 | Ctrl+Z, then type `90` in **Around X** | Same: turns in place. Position X / Y / Z change, which is expected | |
| 6 | Make a Merge: box 20 wide × 40 deep × 20 high, plus a cylinder Radius `5` Height `15` sitting on its top (Z = 20) | One shape 35 tall | |
| 7 | Select it, press **E** | The rings are at Z = 17.5, the middle of the 35 | |
| 8 | Make an Intersect of two overlapping boxes, select it, press **E** | The rings are in the middle of the shared piece only | |

### Align Shapes

Start with **Clear Plate**. Add a box (shape 1). Add a second box, set its size to
10 × 10 × 10 and move it well off to the right (shape 2).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | With one shape selected, look at **Align Shapes** in the toolbar | Greyed out | |
| 10 | Click shape 1, Ctrl+click shape 2, click **Align Shapes** | A list: Meet At Face; Align Face Center XYZ / X / Y / Z; Center XYZ / X / Y / Z. The same list is under **Edit → Align Shapes** | |
| 11 | **Meet At Face** | Shape 2 slides left until it touches shape 1's right side. Height and front/back don't change. Undo reads "Undo Meet At Face" | |
| 12 | Ctrl+Z, then **Align Face Center X** | Shape 2's middle is on shape 1's right face: half of it is inside shape 1 | |
| 13 | Ctrl+Z, then **Align Face Center XYZ** | Shape 2 is centred on the middle of shape 1's right face | |
| 14 | Ctrl+Z, then **Center XYZ** | Shape 2 sits exactly in the middle of shape 1 | |
| 15 | Ctrl+Z, then try **Center X**, **Center Y**, **Center Z** in turn (Undo between) | Only that one direction changes each time | |
| 16 | Move shape 2 above shape 1 (Z about 50), then **Meet At Face** | Shape 2 drops onto shape 1's top | |
| 17 | Select shape 2 **first**, then Ctrl+click shape 1, **Center XYZ** | This time shape 1 moves to shape 2 (the first one picked stays put) | |
| 18 | Add a third box, select shape 1 then Ctrl+click the other two, **Meet At Face** | Both move against shape 1; one Ctrl+Z puts both back | |
| 19 | Align a cylinder to a box with **Meet At Face** from the side | The round side just touches the box: no gap, no overlap | |
| 20 | Select a combined shape and another shape, try any Align option | Works the same, using the visible shape | |

### Sit on plate

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 21 | Turn a cylinder 45° around Z, then 90° around X, then **Sit on plate** | Its curved side rests exactly on the plate | |
| 22 | Open a file saved by 0.5.0 that has a combined shape | Opens as before; turning it now uses the visible centre | |

## Send back

- The results table (or "all pass" plus any failures).
- `forge3d.log` if anything failed.
- Whether **Align Face Center X / Y / Z** does what you meant (halfway in along that axis).
