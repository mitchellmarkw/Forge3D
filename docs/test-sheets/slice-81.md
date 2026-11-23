# Test sheet: 0.90.0, Slice 81: Joint sizes from the design variables

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 81: Joint sizes from the design variables (0.90.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Start with two boxes (20 mm), one on top of the other with a 1 mm gap, and in Build → Variables add **PegDia** = 3,
**Fit** = 0.1, **Gap** = 1.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the lower box; Features → Quick Socket; Socket for: a peg; click the top face | The socket panel | |
| 2 | Click in the peg size box and press **=**; type `PegDia`; OK | The box shows 3.00 and turns blue; hovering shows "= PegDia" | |
| 3 | Same for Fit: `Fit`, and the peg depth: `PegDia + 1` | Both blue | |
| 4 | **Add Matching Part**, then Apply | A peg shape and the socket; the peg's radius and height in Properties are blue | |
| 5 | Variables: PegDia 4; OK | The socket's hole gets bigger and deeper; the peg gets fatter and longer; one undo step | |
| 6 | Double-click the socket step in Objects | The panel shows 4.00 / 5.00, still blue | |
| 7 | Type 3.5 in the peg size box; Apply; Variables: PegDia 3 | The socket stays 3.5 (it no longer follows); the depth still follows (4) | |
| 8 | Undo until the socket's back to PegDia; right-click the fit box → **Stop Following the Variables**; Apply | Fit stays as it is, not blue | |
| 9 | Quick Hinge between the boxes, a double hinge, the other half on the upper box; give Gap `Gap` and the pin `PegDia / 2`; Apply | A fork in each box | |
| 10 | Variables: Gap 1.5 | Both forks change; the upper fork's face moves 0.5 mm up | |
| 11 | Variables: PegDia 12 | Refused: "That would make ... impossible: ..." naming a joint that can't take it; nothing changes | |
| 12 | Hand Ports on a box: a peg; peg size `PegDia`, length `PegDia * 2`; Apply; PegDia 2.5 | The port's peg 2.5 across, 5 long | |
| 13 | Save; close; open again; change PegDia | The joints still follow | |
