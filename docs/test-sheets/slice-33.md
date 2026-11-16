# Test sheet: slice 33, Does it fit the printer? (0.42.0 Beta)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 33: Printer height and fit warning (0.42.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | View tab → **Plate Size** | A new **Height it can print (Z)** box and a **Show shapes that don't fit in orange-red** tick box | |
| 2 | Choose your printer's bed from the list | The height fills in; change it to your printer's real height if it differs. OK | |
| 3 | Look at the plate | A faint box standing on the plate, as tall as the height | |
| 4 | Add a Box and drag it until part of it is over the plate's edge | It turns orange-red; the status bar (bottom right) says "1 shape doesn't fit the printer" | |
| 5 | Drag it back | Its own colour again; the message goes | |
| 6 | Make the box taller than the height (Properties) | Orange-red again; shorter: back to normal | |
| 7 | Put a long thin box near a corner of the plate so it fits, then rotate it 45° about Z so its corner pokes over the edge, and back | Flagged only while a real corner is over the edge | |
| 8 | Plate Size: untick the orange-red box, OK, and push a shape off the plate | No warning colour or message. Tick it again | |
| 9 | Restart Forge3D | The height and the tick box are as you left them | |

## Send back

"All pass" or the failures (with the number).
