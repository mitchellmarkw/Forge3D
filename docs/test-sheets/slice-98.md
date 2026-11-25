# Test sheet: 0.106.0, Slice 98: Show and hide shapes

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 98: Show and hide shapes (0.106.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Use a file with two or three shapes (a figure cut into parts is good).

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Look at Objects | An eye at the left of every shape's row (not on the step rows when you open a shape) | |
| 2 | Click a shape's eye | The shape goes from the view; its row greys out with a crossed-out eye; what was selected stays selected | |
| 3 | Click in the view where it was | The shape behind is picked (or nothing), never the hidden one | |
| 4 | Click the eye again | The shape is back | |
| 4a | Hide a shape, then click its name in Objects | It shows see-through with its move handles; click elsewhere and it's gone again | |
| 5 | Select a shape in the view, Ctrl+H | It's hidden and let go | |
| 6 | Right-click another shape in Objects → Show Only This | Only that shape shows | |
| 7 | Ctrl+Shift+H (or View → Show All) | Everything shows again | |
| 8 | Hide a shape, Export the plate as STL (or Send to Slicer) | The hidden shape is in the file too | |
| 9 | Hide a shape, save, close and open the file | Everything shows | |
| 10 | Hide a shape, select it in Objects, open Paint on it | It shows while painting; hidden again after Done | |
| 11 | Switch to the dark theme | The eyes are light and easy to see | |
