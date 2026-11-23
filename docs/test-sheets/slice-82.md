# Test sheet: 0.91.0, Slice 82: Hinge pieces that follow the hinge

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 82: Hinge pieces that follow the hinge (0.91.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Start with a 20 mm box, and in Build → Variables add **PegDia** = 3.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Features → Quick Hinge on the box's top; Single hinge; The other half: A new tab shape, in place; Printed pin | The panel | |
| 2 | Tab thickness: press = and give `PegDia`; Apply | The fork, plus "Hinge tab 3 × Ø5" and "Hinge pin ..." in Objects | |
| 3 | Variables: PegDia 4; OK | The fork's slot and the tab both get thicker; the tab is now "Hinge tab 4 × Ø5"; one undo step ("Design variables") | |
| 4 | Undo | The slot, tab and name back to 3 | |
| 5 | Redo; move the pin away (drag it, or change its X); Variables: PegDia 3 | The tab back to 3 in place; the pin stays where you moved it | |
| 6 | Rename the tab "My tab"; PegDia 4 | The tab gets thicker, still "My tab" | |
| 7 | Double-click the hinge step; change Pin diameter to 2.5; Apply | The pin is remade 2.5 across, still where you moved it | |
| 8 | A double hinge between two boxes (a 1 mm gap); Gap: `PegDia / 3`; Apply; PegDia 6 | The link and both pins remade for the 2 mm gap; the second fork moved out | |
| 9 | Save; close; open again; change PegDia | The pieces still follow | |
