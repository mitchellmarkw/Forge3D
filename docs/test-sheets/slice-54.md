# Test sheet: slice 54, Magnet pockets (0.63.0 Beta)

Time needed: about 10 minutes, plus a print (check 9) when you can.

## Get the build

Open the pull request **Slice 54: Magnet pockets (0.63.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select a cylinder, Quick Socket, Socket for: **A magnet** | Magnet (label right on top of its drop-down) "6 × 2 mm"; Diameter and Thickness greyed; Hidden under a skin; A matching pocket…; no relief cuts | |
| 2 | Click its top; try 3 × 1, then Other size (type 7 × 2.5) | A round pocket the magnet's size, previewed; with Other the sizes can be typed | |
| 3 | Apply | The history says "Magnet pocket Ø6 × 2"; a 6 × 2 magnet would sit flush (pocket 6.2 across, 2.1 deep with a 0.1 fit) | |
| 4 | Another, **Hidden under a skin** 0.6 | The surface stays closed; Section View shows the pocket 0.6 mm under it; history "…, under 0.6 mm" | |
| 5 | Two cylinders, one standing on the other. Magnet on the lower one's top, tick **A matching pocket**, **Pick the Other Part**, click the upper one, Apply | Both get a pocket, lined up (one undo step); the status bar reminds you about the poles | |
| 6 | Same, with the upper cylinder moved 10 mm up | It says the other part isn't against the face; Apply stays off | |
| 7 | **Add a Stand-in Magnet** | A "Magnet Ø6 × 2" cylinder beside the shapes | |
| 8 | Pose Test with the two cylinders | "Magnet in …" listed, holding the other part (it doesn't turn) | |
| 9 | Print one (open, and buried with a pause) | The magnet fits; the buried one drops in at the pause and is covered | |
| 10 | Save, close, open | The pockets are there; double-click one opens it as a magnet | |
