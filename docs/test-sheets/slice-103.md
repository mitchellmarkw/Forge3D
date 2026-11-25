# Test sheet: 0.111.0, Slice 103: Print-in-place joints

Time needed: about 15 minutes, plus a test print. (Includes slices 99 to 102 until their pull requests are merged.)

## Get the build

Open the pull request **Slice 103: Print-in-place joints (0.111.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a copy of a figure sculpted in one piece, standing straight, arms away from its sides.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Figure Design → Cut into Parts | Under Printer: **Print in place (filament)** | |
| 2 | Tick it | Clearance 0.35 mm shows; Printer turns to FDM (filament); the summary ends "Printed in place: the pieces stay put together." | |
| 3 | Usual Figure, Apply | The figure in pieces, still put together where it was; no separate pins (the hinges' lines say "with Hinge pin") | |
| 4 | Hide the torso (its eye) | Each ball sits inside its socket with a gap all round; the opening is narrower than the ball | |
| 5 | Look at an elbow from the side, hide the upper arm | The tab's hole is wider than the pin, with a gap all round | |
| 6 | Undo; pick an elbow, Joint type Double hinge, Apply | One separate piece: the hinge link, held on two pins | |
| 7 | Export everything as one STL (or Send to Slicer) and slice it | The slicer shows the pieces apart (no joined walls at the joints) | |
| 8 | Print it (small is fine), free each joint gently | The joints move and don't come apart | |
| 9 | Close Cut into Parts, open it again | Print in place and the clearance are remembered | |
| 10 | Untick Print in place | Cut into Parts works as before (separate pins) | |
