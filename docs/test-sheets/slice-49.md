# Test sheet: slice 49, Reference Images (0.58.0 Beta)

Time needed: about 20 minutes. Have two pictures ready: a front photo or drawing of
something whose size you know (a figure's model sheet, say), and any second picture.

## Get the build

Open the pull request **Slice 49: Reference Images (0.58.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | View → Pictures → **Reference Images** | The panel: Direction (label right on top of the drop-down), "No picture for this side yet", Add Picture, Look From This Side, Done | |
| 2 | Direction **Front**, **Add Picture…**, choose your front picture | The view turns to the front; the picture stands on the plate behind the model, a bit bigger than it, the right way up, outlined in blue. The list says "Front (has a picture)" | |
| 3 | Add a Box (or open a figure) and turn the view | The model is always in front of the picture; the picture is only seen from the front side and fades away as you turn edge-on to it | |
| 4 | Drag the **Opacity** slider | The picture gets fainter / more solid as you drag; one Ctrl+Z puts it back as it was | |
| 5 | Type a **Width**, **Right**, **Up**, **Behind**, **Turn** | The picture changes size (height follows), slides, moves back, turns | |
| 6 | Drag the picture in the view | It slides in its own plane, following the mouse | |
| 7 | Type a length you know on the picture (e.g. a figure's height) with the right unit, scale 1. **Pick Two Points**, click the two ends | A dot at the first click, a line following the mouse to the second; then "Sized: those points are now … mm apart". The first point stays where it was | |
| 8 | Look From This Side, add a Box as tall as the length you typed and stand it beside those two points | The box's height matches the two points on the picture | |
| 9 | Size again with a real length and a scale, e.g. 4.5 m at scale 48 (the panel says 93.75 mm on the model) | The two points are now 93.75 mm apart | |
| 10 | Add a picture for **Right** (or Top). Click **Look From This Side** | The view turns to that side; that picture is the outlined one | |
| 11 | Close the panel (**Done**). Click on the front picture where no shape is in front | The panel opens on the Front picture; the status bar says how to click through it | |
| 12 | Close it. Drag the front picture (where no shape is in front) | It moves like a shape | |
| 13 | Open the panel, untick **Active** on Front, Done. Click and drag on the picture | Nothing picks it: a click clears the selection, the shapes behind or in front can be clicked and moved as normal | |
| 14 | Untick **Show** on a picture; then View → **Show Pictures** off and on | The picture hides (it's still listed). Show Pictures hides and shows all of them | |
| 15 | **Replace Picture…** with another file | The new picture takes the old one's width and place | |
| 16 | **Remove**, then Ctrl+Z | It goes, and comes back | |
| 17 | Save, close, open the file | The pictures are back, with their size, place, opacity, Show and Active | |
| 18 | Export an STL/3MF and Send to Slicer | No picture in the exported model | |
| 19 | Save a custom shape (Print/Save → Library) and add it to a new plate | No picture comes with it | |
| 20 | A big phone photo (several MB, taken upright) | It shows the right way up; the app stays quick on your machine | |
