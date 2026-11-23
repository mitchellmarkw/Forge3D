# Test sheet: 0.86.0, Slice 77: Design variables

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 77: Design variables (0.86.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Start a new design.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Build tab → **Variables** | The Design Variables window: Name, Value or formula, Works out to, Used by, Note | |
| 2 | Add Variable: name **PegDia**, value **3**. Add **Hole** = `PegDia + 2*Fit` | Hole shows "Problem" in red; the line under the table says there's no variable called Fit; OK and Apply are greyed out | |
| 3 | Add **Fit** = `0.1`; OK | Hole works out to 3.2; the window closes | |
| 4 | Add a Cylinder; click in Radius X and press **=** | A small window "Radius X =" with the variables to choose from | |
| 5 | Type `Hole / 2` (or pick Hole from the list and type ` / 2`) | "Works out to 1.6" under it; OK | |
| 6 | Look at Properties | Radius X and Radius Y are 1.60 mm, both tinted blue; hovering shows "= Hole / 2" | |
| 7 | Add a Box; right-click its Width → **Use a Variable...**; type `PegDia * 4`; OK | Width 12 mm, tinted blue | |
| 8 | Variables: change PegDia to 4; OK | The cylinder's radius becomes 2.10 mm and the box's width 16 mm; the status bar says 2 shapes changed; Used by shows PegDia 2 shapes, Hole 1 shape | |
| 9 | Undo | Back to radius 1.60 and width 12; PegDia is 3 again in the window | |
| 10 | Redo; in Variables rename PegDia to **Peg**; OK, then open Variables again | Hole's formula now reads Peg + 2*Fit; the box's Width tooltip shows = Peg * 4; nothing changes size | |
| 11 | Variables: Peg = `-2`; OK | Refused: "That would make ... impossible", and the window stays open | |
| 12 | Type **5** into the cylinder's Radius X | Radius 5 mm, no longer blue; the status bar says Radius X and Radius Y no longer follow the design variables | |
| 13 | Right-click the box's Width → **Stop Following the Variables** | Width stays 16 mm, no longer blue | |
| 14 | Give the box's X position `Peg * 5`; Save; close; open the file again | The variables are in the window; X is still blue, = Peg * 5 | |
| 15 | Variables: select Peg, Delete | It asks first (the box's X uses it); say Yes: Hole shows a problem (no variable called Peg) and OK is greyed out. Cancel | |
| 16 | Help → Figure, Toy and Kit Tools | A "Design Variables" section | |
