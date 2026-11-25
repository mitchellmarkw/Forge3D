# Test sheet: 0.105.0, Slice 97: Sculpt strength

Time needed: about 10 minutes. (Includes slices 95 and 96 until their pull requests are merged.)

## Get the build

Open the pull request **Slice 97: Sculpt strength (0.105.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a file with a sculpted shape, or add a box and sculpt a few Draw strokes on top.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Click the sculpt step (Objects list, or the shape's history) | Properties shows **Sculpt strength** with a slider and **100 %** | |
| 2 | Click a shape that isn't sculpted | No Sculpt strength row | |
| 3 | Back on the sculpt step, drag the slider to about 50% and let go | The sculpting is about half as strong; the history line says "at 50%" | |
| 4 | Type 0 in the percent box, Enter | The shape as it was before the step (the strokes' marks gone) | |
| 5 | Type 150, Enter | Every stroke stronger than as sculpted, by about half again | |
| 6 | Undo three times | Back to 100%, exactly as sculpted | |
| 7 | Set 60%, save, close and open the file | Still at 60%, the same shape | |
| 8 | With the step at 60%, open it to sculpt more (double-click), add a stroke, Done | The step is still at 60%, the new stroke toned down with the rest | |
| 9 | On a heavy sculpt (a figure), drag the strength | The change shows within a moment of letting go (no long wait) | |
| 10 | Sculpt with Remesh partway through, then change the strength | Only the strokes after the remesh tone down; those before it stay as they were | |
