# Test sheet: 0.93.0, Slice 84: Faster Make Room

Time needed: about 10 minutes. (Includes slices 82 and 83 until those pull requests are merged.)

## Get the build

Open the pull request **Slice 84: Faster Make Room (0.93.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a jointed figure you've used Make Room on before (a detailed one is best), and the version before
this one to compare with if you like.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Pose Test; pick a knee or elbow; Make Room for it to 120° (or past where it stops) | Done noticeably quicker than before | |
| 2 | Look at the cut | The same shape of cut as before: smooth, only where it was in the way | |
| 3 | Turn the joint to the angle | It turns there without hitting | |
| 4 | Undo | The cut goes | |
| 5 | Make Room with "cut from the turning part" | Quick too; the cut on the turning part | |
| 6 | A ball joint (shoulder): Make Room for its bend and its twist | Works as before | |
