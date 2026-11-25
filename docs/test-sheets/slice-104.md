# Test sheet: 0.112.0, Slice 104: Rough edges

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 104: Rough edges (0.112.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | New file; add a Box, then another Box | "Box", then "Box 2" | |
| 2 | Delete "Box"; add a Box | "Box" again | |
| 3 | Subtract a Cylinder from a Box | The result is "Subtract"; the cylinder was "Cylinder" | |
| 4 | Pose Figure on a figure, Apply | The step is "Posed"; a second posed figure's step "Posed 2" | |
| 5 | Accessories → Make two swords | "Sword", then "Sword 2" | |
| 6 | Open a file saved before this version | Its shapes keep their names ("Box 1" stays "Box 1") | |
| 7 | Cut into Parts; pick the Left shoulder (Ball joint); tick Print in place | The line under the joint says "clearance 0.35 mm" and that the ball is held in | |
| 8 | Change the clearance to 0.45 | The line says 0.45 at once | |
| 9 | Pick the Left elbow (Hinge) | The line says the clearance and "The pin is part of the fork" | |
| 10 | Untick Print in place | The line shows the usual fit again | |
