# Test sheet: slice 11b, overlapping pattern copies (0.20.1)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Pattern: overlapping copies no longer hang (0.20.1)**, open its
Windows build check, download the artifact, unzip and run `Forge3D.exe`.
**Help → About** says 0.20.1.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Cylinder. **Follow**: a short line across its side, Round groove, Apply | One groove | |
| 2 | Design → **Pattern**: Its last step, **Round a circle** (line straight up through its middle), How many 4 | Within a few seconds, the preview shows 4 grooves round it | |
| 3 | **Apply** | "Pattern 1  circle of 4 (…)"; no ⚠; the shape looks sound | |
| 4 | Double-click the Follow step, make the line long enough that the grooves overlap (most of the way round), Apply | Within a few seconds the 4 grooves join into one ring round the cylinder; no ⚠ | |
| 5 | Double-click Pattern 1, How many 12, Apply | 12 grooves (or a ring); Forge3D doesn't freeze for more than about 30 seconds. If it takes that long you see "Making the copies took too long…" and the step is marked ⚠ instead | |
| 6 | Undo back to 4, **File → Export STL** | The file exports | |
| 7 | Slice 11 checks 15 and 17–18 again | As before | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
