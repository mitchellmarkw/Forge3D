# Test sheet: slice 50, Quick Hinge (0.59.0 Beta)

Time needed: about 30 minutes, plus printing checks 15–17 if you can.

## Get the build

Open the pull request **Slice 50: Quick Hinge (0.59.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Cylinder. Select it, Features → **Quick Hinge** | The panel: Joint, The other half (labels right on top of their drop-downs), tab sizes, Pin, Stiff, Bends, Turn, Reach, Center on the face, Printer, Fit | |
| 2 | Click the top face | The fork is shown cut into the top: a slot with a pin hole across it. In blue: the tab standing in the slot and the pin, as long as the cylinder is wide | |
| 3 | **All** under Center on the face; **+90°** | The joint moves to the middle of the face; the slot and pin turn a quarter round | |
| 4 | Bends: **One way (forward)** | The slot is cut deep on one side only | |
| 5 | Pin: **Filament (1.75 mm)**, then **Wire or rod** with 1.5 mm, then **Printed pin** again | The diameter shows 1.75 and can't be changed; then 1.5; back to printed, it's as it was. No pin in blue for filament or rod | |
| 6 | Other half: **A new tab shape, in place**. **Apply** | One undo step: "Hinged 1" (Fork in its history), "Hinge tab 3 × Ø5" and "Hinge pin Ø2 × …" shapes in place. Ctrl+Z takes it all back | |
| 7 | Move the tab up out of the slot and look at it | A tab with a rounded end and a pin hole; Merge it onto another part, and the pin fits through both | |
| 8 | Add two cylinders, one standing on the other. Quick Hinge on the lower one, click its top face (look from above: the upper one doesn't get in the way). Other half: **Made from another part**, **Pick the Other Part**, click the upper cylinder, Apply | The upper cylinder's end becomes the tab, down in the fork; it stands just clear of the lower one's face. Both are named Hinged; the pin is added | |
| 9 | Double-click the fork step (Objects) and change Tab thickness | Apply changes both halves (the tab's description in Objects shows the new size) | |
| 10 | Joint: **Double hinge**, Other half: **Fork another part too**, pick the other part (put a 1 mm gap between the parts first) | Both parts get a fork, facing each other; a "Hinge link" with two pin holes and two pins are added, lined up | |
| 11 | Joint: **Swivel** (Peg diameter 4, Peg goes in 5, Lip 0.4) on a cylinder's top, Other half: a new peg shape | A bore with a groove at the bottom; a "Swivel peg Ø4" with a lip and a slit across its end | |
| 12 | Swivel made from another part | The other part gets the peg on its end, standing just clear | |
| 13 | Quick Hinge on one of your figure meshes (a thigh's end), Apply | It's made; on a very big mesh the cut shows in red first. Note how long Apply takes | |
| 14 | Save, close, open the file | The joints are there; double-click still opens them; changing one still changes its partner | |
| 15 | Print 6 (tab, fork, pin) in resin or FDM (Printer set to match) | The pin goes through the arms snugly, the tab turns on it and holds a pose (Stiff) | |
| 16 | Print it again with Stiff off | It turns freely | |
| 17 | Print the swivel of 11 | The peg snaps in (the slit lets the lip squeeze through), turns, and doesn't pull out | |
