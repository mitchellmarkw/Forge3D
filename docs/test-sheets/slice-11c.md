# Test sheet: slice 11c, long Follow grooves (0.20.2)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Follow: long grooves cut cleanly (0.20.2)**, open its Windows
build check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says
0.20.2.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Cylinder (R20 × 40). **Follow**: Groove, Round, 4 × 2, a short line across its side, Apply | One groove cut in | |
| 2 | Double-click the Grooved step, **Draw the path...**, make the line about 100 mm long (most of the way round), finish, Apply | A longer groove cut **into** the side, not a tube standing out of it | |
| 3 | Again, make the line about 140 mm (past once round) | "The path runs back over itself…" in red; Apply greyed | |
| 4 | Back to about 100 mm; Make: **Ridge**, Apply | A ridge standing out along the same path | |
| 5 | Back to Groove. **Pattern** its last step round a circle (line straight up through its middle), 4, Apply | Within a few seconds the grooves join into a ring cut into the side; no ⚠ | |
| 6 | Double-click the Grooved step (inside the Pattern), make the line about 60 mm, Apply | The ring updates; still cut in; no ⚠ | |
| 7 | Slice 11b checks 1–6 again | As before | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
