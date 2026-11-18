# Test sheet: slice 46, Quick Socket (0.55.0 Beta)

Time needed: about 25 minutes, plus printing for check 13. Merge slice 45 (custom
shapes) first: this build includes it.

## Get the build

Open the pull request **Slice 46: Quick Socket (0.55.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box. Select it, Design → **Quick Socket** | The panel opens: Socket for (A ball), Ball diameter, Hold, Grip, Turn, Printer, Fit, Relief cuts, Add the Matching Ball. Each drop-down has its label right above it | |
| 2 | Set Ball diameter to 8 and click the middle of the box's top | A round opening appears there, narrower than 8 mm; "This is how it will look." | |
| 3 | Change Hold to **Open** | The opening widens to the full ball; the Grip box goes away | |
| 4 | Click the box's front face | The socket moves there, square to that face | |
| 5 | **Apply** | The box gets the socket; Objects shows the step "Ball socket Ø8 (open)". Undo takes it away, Redo puts it back | |
| 6 | Double-click the socket step in Objects | The panel opens with its settings. Set Hold back to Snap-in, Apply: it changes | |
| 7 | Quick Socket again on another box: Socket for **A peg**, Peg shape **Hexagon**, Across flats 6, Length 4 | A hexagonal hole 4 mm deep. **Turn** 30° turns the flats | |
| 8 | Relief cuts **4** | Four thin slots out from the opening. Cut depth **Auto** reaches half way down; type a depth and they change | |
| 9 | **Add the Matching Peg** | "Hexagon peg 6 × 4" appears beside the shapes (a 6 mm hexagon, 4 mm tall). Undo says "Undo Add Hexagon peg 6 × 4" | |
| 10 | Printer **FDM**, then **Resin** | The Fit changes to 0.20 mm, then back to 0.10 mm. You can still type your own fit | |
| 11 | Quick Socket on one of your big imported figure meshes (a million triangles or more), at a shoulder | The cut shows in red while you set it up (the panel says why). Apply cuts it; note how long that takes | |
| 12 | Start Quick Socket again on anything | It starts with the settings you used last (even after restarting Forge3D) | |
| 13 | Print a ball socket and its matching ball (from Add the Matching Ball) on your resin printer, and a hex peg socket and peg | The ball snaps in and holds; the peg slides in and can't turn. Note the fit and grip you'd change for your printer | |
| 14 | Save, close and open the file again | The sockets are all there, and can still be changed | |
| 15 | Select a shape with a socket and resize it evenly to twice its size with the Resize handles (R), Uniform on | The socket doubles too; its fit stays the same | |
