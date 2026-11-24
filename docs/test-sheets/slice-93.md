# Test sheet: 0.101.0, Slice 93: Faster Split by Mask

Time needed: about 10 minutes. (Includes slice 92 until its pull request is merged.)

## Get the build

Open the pull request **Slice 93: Faster Split by Mask (0.101.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of your shoulder figure (the big one), in 0.99 and in this build, to compare.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | In **0.99**: Split by Mask on the arm, painting as you did before (small brush, many strokes), Curved; time Apply | (Note how long it takes, and the triangle count at the bottom right afterwards) | |
| 2 | In **this build**: the same | Noticeably quicker; far fewer triangles afterwards | |
| 3 | Hide the body | The arm's cut is closed and smooth, following the paint's edge as closely as before | |
| 4 | Undo; the same paint, Flat; Apply | Quick; a flat cut at the shoulder | |
| 5 | Undo; Ball and socket; Apply | Quick; dome and socket as before | |
| 6 | Undo; Flat with Joint at the cut, Ball joint; Apply | Quick; the joint at the shoulder | |
| 7 | Paint a small dab in the middle of a big flat face of a Box; Apply | A small piece cut out where you painted (not missed) | |
| 8 | Save, close, open | The pieces as they were | |
