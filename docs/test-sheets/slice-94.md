# Test sheet: 0.102.0, Slice 94: A joint at a split follows its cut

Time needed: about 15 minutes. (Includes slices 92 and 93 until their pull requests are merged.)

## Get the build

Open the pull request **Slice 94: A joint at a split follows its cut (0.102.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Add a Cylinder about 12 mm across and 40 tall.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Split it across the middle, Joint at the cut **Ball joint** | Two pieces, a ball on the bottom one, a socket in the top one | |
| 2 | Expand the top piece in Objects; select its **cut in two** step | Properties shows **Move the cut...** | |
| 3 | Move the cut...; From the middle **6 mm**; Apply | Both pieces cut 6 mm higher; the ball and socket moved up with the cut (move the top piece off to see) | |
| 4 | Undo (once) | Back as in 1 | |
| 5 | Repeat 1–3 with **Hinge** | The fork, tab and pin all at the new cut; the old pin gone | |
| 6 | Repeat with **Magnets** | The pockets at the new cut | |
| 7 | Split by Mask: paint the top end off, Flat, Joint at the cut Ball joint; Apply | Socket in the painted piece, ball on the rest | |
| 8 | Change its mask (double-click its step): paint further down; Apply | Both pieces cut at the new edge, the joint with them | |
| 9 | Undo | Back as in 7 | |
| 10 | Delete the joint's steps by hand (or Remove the joint), then Move the cut | The cut moves; no joint is put back | |
| 11 | Save with a jointed split, close, open, Move the cut | The joint still follows | |
