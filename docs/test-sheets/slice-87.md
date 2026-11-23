# Test sheet: 0.96.0, Slice 87: Cut surfaces for Split by Mask

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 87: Cut surfaces for Split by Mask (0.96.0)**, open its Windows build check, download
the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure with an arm or leg, or add a cylinder (about 12 mm across, 40 tall).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the arm; Design → Split by Mask | The panel has **Cut surface:** (label above the list), "Curved (follows the mask)"; **Socket fit** greyed | |
| 2 | Paint the hand end off with a slanted edge; Cut surface **Flat**; Apply | Two pieces; hide the hand: the cut on the arm is flat (not curved like before) | |
| 3 | Undo; Split by Mask again, paint the same, **Ball and socket** | Alignment keys and Key fit grey out; Socket fit (0.15 mm) can be set | |
| 4 | Apply | The hand has a low dome under it; the arm has a matching socket (hide one piece to see each) | |
| 5 | Move the hand off and back on | The dome sits in the socket, a small gap round it | |
| 6 | Double-click the hand's step ("… (masked)") in Objects | The mask panel shows Ball and socket, Socket fit 0.15 | |
| 6b | Set Socket fit 0.3; Apply | Both pieces remade, the socket a little bigger | |
| 7 | Double-click the step again; Cut surface Curved; Apply | The curved cut back, keys as ticked | |
| 8 | Save, close, open | The pieces as they were | |
| 9 | Paint a band round the middle of a cylinder; Flat; Apply | A message: the paint's edge goes round twice; use the curved cut. Nothing changed | |
| 10 | Same band with Curved | Cuts as before | |
