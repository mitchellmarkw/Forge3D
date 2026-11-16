# Test sheet: slice 29, Taper (0.38.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 29: Taper (0.38.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, **Sketch**. Draw a rectangle about 30 × 20 and a circle inside it | Under **Length:** a new **Taper:** box at 0.0° (its label above it) | |
| 2 | Taper 15, Length 10, **Finish** | A block whose sides lean in towards the top; the hole is wider at the top than at the bottom. Objects: "Pad 1  sketch, 10 mm, 15° taper" | |
| 3 | Double-click Pad 1, Taper -15, Finish | The sides lean out instead (wider at the top; the hole narrower at the top); "15° taper out" | |
| 4 | Double-click, Taper 0, Finish | Straight sides, as pads always were | |
| 5 | Double-click, Taper 60, Length 40, Finish | A message that the taper is too steep for this shape; still in the sketch. Set Taper 10, Length 10, Finish | |
| 6 | Double-click, tick **Both ways**, Finish | It narrows both up and down from the middle | |
| 7 | Add a Box 40 × 40 × 20. Select it, **Sketch**, click its top. Draw a rectangle, **Make: Pocket**, Depth 8, Taper 20, **Finish** | A pocket with sloping walls: narrower at the bottom than at the opening | |
| 8 | Double-click the pocket step; tick **Through all** | Taper greys out; Finish cuts straight through | |
| 9 | Select the tapered pad from check 6, **R**, drag the top handle to make it taller | It gets taller and keeps its shape (the top stays the same size) | |
| 10 | Save, close, open | The tapered pad and pocket as they were | |

## Send back

"All pass" or the failures (with the number).
