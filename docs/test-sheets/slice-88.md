# Test sheet: 0.97.0, Slice 88: A joint at a split

Time needed: about 15 minutes. (Includes slice 87 until its pull request is merged.)

## Get the build

Open the pull request **Slice 88: A joint at a split (0.97.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Add a Cylinder (about 12 mm across, 40 tall) to try it on, or use a copy of a figure's arm or leg.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select it; Design → Split | A **Joint** box above Alignment pegs: "Joint at the cut:" (label above the list) set to None | |
| 2 | Joint at the cut: **Ball joint** | Ball size "Fit the cut" and Joint fit can be set; the Alignment pegs grey out | |
| 3 | Split | Two pieces. Move the top piece up: a ball on a neck stands out of the bottom piece, a socket in the top one; the bottom piece is a little shorter | |
| 4 | Put the top back on (Undo the move) | The ball is inside the socket | |
| 5 | Undo the split; Split again with **Hinge** | A fork in one piece, the other's end made into a tab, and a pin beside them | |
| 6 | Undo; again with **Double hinge** | Two forks, a link and pins | |
| 7 | Undo; again with **Swivel** | A peg on one piece, a bore in the other | |
| 8 | Undo; again with **Magnets** | A magnet pocket in each cut face, facing each other | |
| 9 | Undo; Ball joint with Ball size 20 mm; Split | A message saying the cut is too small for a 20 mm ball and how much it needs; nothing changed | |
| 10 | Keep: Only the top | The Joint box greys out (a joint needs both halves) | |
| 11 | Cancel; Split by Mask; paint the top end off; Joint at the cut: Ball joint; Apply | Two pieces: the socket in the painted piece, the ball on the rest; Alignment keys greyed while a joint is chosen | |
| 12 | Undo; Split by Mask, Cut surface **Ball and socket** | Joint at the cut greys out | |
| 13 | Close Forge3D, open it, Split | The joint you chose last is still chosen | |
| 14 | Save, close, open | The jointed pieces as they were | |
