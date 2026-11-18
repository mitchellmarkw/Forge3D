# Test sheet: slice 45, Custom shape library (0.54.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 45: Custom shape library (0.54.0)**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere and set Radius X to 1.5 mm. Print/Save → **Save Custom Shape** (in the new Library group) | A box asks for a name, with the shape's name filled in | |
| 2 | Type `Ball joint 3mm`, OK | The status bar says it's saved. The shape in your model keeps its own name | |
| 3 | Add Shape → **Custom** (on the Build tab, and in the Add menu) | "Ball joint 3mm" is listed, with Manage Custom Shapes... below it | |
| 4 | Pick it | "Ball joint 3mm" appears beside the other shapes, radius 1.5 mm, sitting on the plate. Undo takes it away in one step | |
| 5 | Start a new model (File → New) and add it twice | Both at radius 1.5, side by side, called "Ball joint 3mm" and "Ball joint 3mm 2" | |
| 6 | Make a socket: a Box with a Cylinder (radius 1.6) subtracted from it. Save it as `Socket 3.2mm`, then add it to a new model | It comes in still made with Subtract: open it in the Objects list, pick the cylinder and change its radius, and the hole changes | |
| 7 | Select two shapes a little apart (say the ball and the socket) and save them together as `Ball and socket` | It asks for a name for "these 2 shapes". Added, both come in, the same distance apart, each with its own name | |
| 8 | Save something under a name that's already used | It asks whether to replace it. No lets you type another name; Cancel stops | |
| 9 | Sculpt a shape (or use one of your big imported meshes) and save it. Add it to a new model | It comes in quickly, without the long wait for the result to be worked out | |
| 10 | Custom → **Manage Custom Shapes...** | Your shapes are listed, and the folder at the bottom is in your Documents (Documents\Forge3D\Custom Shapes) | |
| 11 | Rename one, then add it from the Custom menu | The new name is in the list and the menu, and the added shape has the new name | |
| 12 | Delete one | It asks first; then the file is in the Recycle Bin and the shape is off the list and the menu | |
| 13 | **Open Folder** | Explorer opens the folder: one .f3d file per shape (they open in Forge3D like any model) | |
| 14 | **Change Folder...** and pick another folder (for example one in your SynologyDrive) | The list shows that folder's shapes (empty at first). Saving now goes there, and it's remembered after restarting | |
| 15 | Save one of your own figure parts (a joint or peg you reuse) and add it to another figure | It comes in at exactly the saved size: check with Measure | |
