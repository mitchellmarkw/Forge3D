# Test sheet: 0.110.0, Slice 102: Reshape box

Time needed: about 10 minutes. (Includes slices 99 to 101 until their pull requests are merged.)

## Get the build

Open the pull request **Slice 102: Reshape box (0.110.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box; Sculpt tab → **Reshape** | A box of gold handles and lines round it (3 × 3 × 3); the Reshape panel | |
| 2 | Look from the front; drag the top middle handle up | The top bulges up in the middle, smoothly | |
| 3 | Drag a top corner handle to the right | With "Move left and right together" ticked, the other top corner goes left: the top widens | |
| 4 | Untick it; drag a corner | Only that corner moves | |
| 5 | Handles: Corners only | The handles reset to the 8 corners; dragging one leans or stretches the box | |
| 6 | Put the Handles Back | The box as it was | |
| 7 | Bend it, Apply | A Reshape step ("3 × 3 × 3 handles, n moved"); one Undo takes it away | |
| 8 | Double-click the step | The handles where you left them; move one more, Apply | |
| 9 | Change the box's size in its history | The reshaping goes on from the new size | |
| 10 | Reshape a figure: widen the shoulders with a 4 × 4 × 4 box | The figure bends smoothly, both sides alike | |
| 11 | Esc while reshaping | Cancelled, nothing changed | |
| 12 | Save, close, open | The same shape | |
