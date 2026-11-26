# Test sheet: 0.118.0, Slice 110: Forge3D Scenery groundwork

Time needed: about 15 minutes (this sheet, then Scenery's own: docs/scenery/test-sheets/scenery-0.1.md, in the
Scenery download's test-sheets folder).

## Get the build

Open the pull request **Slice 110: Forge3D Scenery groundwork (Forge3D 0.118.0, Scenery 0.1.0)**, open its
Windows build check. There are now **two** test downloads: `Forge3D-windows-x64-…` and
`Forge3D-Scenery-windows-x64-…` (and an installer for each).

Forge3D's window code was reorganised so the two apps can share the 3D view. Nothing should look or work
differently in Forge3D: this sheet checks that.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run Forge3D | Title, ribbon, view and view cube as before; version 0.118.0 Beta | |
| 2 | Open a figure; orbit, pan, zoom; the view cube's faces and turn arrows | All as before | |
| 3 | Select, move with the gizmo, undo | As before | |
| 4 | Sculpt a few strokes | As before | |
| 5 | View → Theme → Dark, then back | As before | |
| 6 | Help → Check for Updates | "You have the newest Forge3D" (0.117.0 is older) | |
| 7 | Brick Figure, with its preview | As before | |
| 8 | Close Forge3D; look in %LOCALAPPDATA%\Forge3D\logs | Today's log written as before | |
| 9 | Now Scenery's sheet (scenery-0.1.md) | | |
