# Test sheet: slice 47, Join Parts (0.56.0 Beta)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 47: Join Parts (0.56.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box and a Cylinder. Design → Arrange → **Join Parts** | The panel: "1. On the part that moves" and "2. Where it goes", Gap, Across, Across (other way), Turn, Flip | |
| 2 | Click the cylinder's round side, then the box's front face | Step 1 says "Peg Ø20", step 2 "Flat face". The cylinder is shown with its end flat against the middle of the box's front | |
| 3 | Type a Gap of 5, then a Turn of 90 | It stands 5 mm off the face; turning spins it round its own line | |
| 4 | Tick **Flip** | It turns end for end | |
| 5 | **Apply**, then Undo | It moves; Undo ("Join Cylinder 1 to Box 1") puts it back | |
| 6 | Box 20 × 20 × 10 with a Quick Socket peg socket (round, 3 mm, 4 long) in its top, and its matching peg (Add the Matching Peg). Join Parts: click the peg's round side, then click into the socket | Step 2 says the socket ("Round peg socket Ø3 × 4"), the Gap starts at the socket's fit, and the peg sits in the socket with its end level with the top | |
| 7 | The same with a ball socket and its matching ball (click the ball, then into the socket) | The ball sits in the socket, its middle where the socket holds it | |
| 8 | Subtract a cylinder from a box to make a hole, then join a narrower cylinder (peg) into it: click the peg's side, then the hole's side wall | The peg goes in to the bottom of the hole (Gap 0), centred | |
| 9 | One of your figure parts with a ball (a sculpted or imported mesh): Join Parts, click the ball | Step 1 says "Ball Ø…" with about the right size | |
| 10 | Join that ball into a ball socket on another part (a Quick Socket, or a round cup in a mesh) | It sits in the socket; if the part points the wrong way, Turn or Flip puts it right | |
| 11 | Click a flat spot on a mesh part, then a face on another part | Step 1 says "A spot on the surface"; the parts meet there, facing each other | |
| 12 | **Pick Again** on step 1, click elsewhere on the moving part | Step 1 changes; the preview follows | |
| 13 | Esc (or Cancel) partway through | Nothing moves; the panel closes | |
