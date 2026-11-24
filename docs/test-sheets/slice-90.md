# Test sheet: 0.98.1, Slice 90: Flat cuts with uneven paint

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 90: Flat cuts with uneven paint (0.98.1)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use the figure from your screenshot (the shoulder), or any figure with an arm.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Split by Mask; paint the arm off at the shoulder as before (a small brush, many strokes) | | |
| 2 | Cut surface **Flat**; Joint at the cut None; Apply | No "goes round 8 times" message. Two pieces: the arm, and the body | |
| 3 | Hide the body | The arm's cut is flat, across the arm where the paint met the shoulder | |
| 4 | Show the body; hide the arm | The body's cut is flat and matches; none of the body went with the arm | |
| 5 | Undo; same paint, plus a dab or two on the chest (away from the arm); Flat; Apply | Same as 2–4: the specks on the chest are ignored | |
| 6 | Undo; same paint; **Ball and socket**; Apply | A dome on the arm's cut, a socket in the shoulder, in the middle of the cut | |
| 7 | Undo; same paint; Flat with Joint at the cut **Ball joint**; Apply | The socket in the arm, the ball on the shoulder, in the middle of the cut | |
| 8 | Undo; Curved with Joint at the cut Ball joint; Apply | The joint at the shoulder (not at a speck) | |
| 9 | On a plain cylinder, paint a band round the middle only; Flat; Apply | Still refused: "goes round 2 times, like a band" | |
