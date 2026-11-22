# Test sheet: 0.83.0, Slice 74: Linked left/right parts

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 74: Linked left/right parts (0.83.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure built from parts: a torso and a separate left arm (named "Left arm").

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the left arm, Design tab → **Mirror** → **Linked Copy on the Other Side...** | A small window: "Mirror:" (label right on top) Left / right, and "Mirror at:" the middle of the other shapes (the torso) | |
| 2 | OK | "Right arm" on the other side of the torso, the same colour; the list says "Mirror of 'Left arm', left / right, kept up to date" | |
| 3 | Change the left arm's length in Properties | The right arm changes to match straight away | |
| 4 | Move the left arm further out | The right arm moves out the other way | |
| 5 | Sculpt the left arm (a bump), Done | The bump shows on the right arm, mirrored | |
| 6 | Hand Ports: a peg on the left arm's wrist | The right arm gets the peg too | |
| 7 | Move the right arm a little | It moves; later changes to the left still show on it there | |
| 8 | Double-click the right arm in the list; change Mirror at; OK | It moves to the new mirror; Undo puts it back | |
| 9 | Delete the left arm | The right arm goes empty with a warning in the list; Undo brings both back | |
| 10 | Save, close, open again; change the left arm | Still linked | |
| 11 | Select the right arm, **Unlink** (Properties or Mirror menu) | A plain shape "Right arm" exactly as it was; changing the left arm no longer changes it; Undo links it again | |
| 12 | Resize the right arm (before unlinking) | A message: it follows the shape it mirrors | |
| 13 | Help → Figure, Toy and Kit Tools | A "Linked left and right parts" section | |
