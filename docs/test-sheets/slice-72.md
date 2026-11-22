# Test sheet: 0.81.0, Slice 72: Make Room

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 72: Make Room (0.81.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure (or two blocks) with a Quick Hinge knee that can't bend far, and one with a ball joint.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Features → **Pose Test**; choose the knee; **Find Range** | How far it turns each way before parts hit (less than 90° forward) | |
| 2 | Under the joint: "Make room to turn further" | Turn to (90°), Clearance (0.30 mm), "Cut it from:" with its label right on top, **Make Room** | |
| 3 | Cut it from **The part it hits**; **Make Room** | The button says "Making Room..." for a moment; then a line saying how much was cut away; the thigh's history shows "minus Room for Hinge in … to 90°" | |
| 4 | Turn the knee to 90° (type it or drag) | "Clear: no parts hit"; **Find Range** now says at least 90° forward | |
| 5 | Look at the cut | A smooth curved notch just where the shin swings through; nothing cut elsewhere | |
| 6 | Undo | The thigh as it was (one step) | |
| 7 | Cut it from **The part that turns**; Make Room | The shin's end is trimmed instead; the knee reaches 90° | |
| 8 | Turn to **−45°** | Room made for turning the other way | |
| 9 | Turn to an angle it already reaches | "Nothing's in the way: it turns that far already." | |
| 10 | A ball joint (a shoulder): "Which way:" appears; choose Bend, 90°, Make Room | The torso (or the arm) trimmed so it bends that far | |
| 11 | Print a cut knee (or check in your slicer) | It bends to 90° with the clearance to spare | |
| 12 | Save, close, open again | The cuts come back the same | |
