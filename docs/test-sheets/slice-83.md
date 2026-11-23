# Test sheet: 0.92.0, Slice 83: Moving by hand stops following the variables

Time needed: about 5 minutes. (Includes slice 82's changes until that pull request is merged.)

## Get the build

Open the pull request **Slice 83: Moving by hand stops following the variables (0.92.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Add a box, and in Build → Variables add **PegDia** = 3.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Give the box's Width `PegDia * 4` and its Z (up) `PegDia` (press = in each) | Width 12, Z 3, both blue | |
| 2 | Resize the box's width with its handle | It resizes; Width isn't blue any more; the status bar says "Width (X) no longer follows the design variables" | |
| 3 | Variables: PegDia 4 | The width stays as you dragged it; Z goes to 4 | |
| 4 | Undo twice | Width back to 12 and blue again | |
| 5 | **Sit on plate** | Z goes to 0, not blue; the status bar says Z no longer follows | |
| 6 | Give X a formula; select it and another shape; Align (left edges, say) | X stops following if it moved; the status bar says so | |
| 7 | Turn it with the ring handle when Turn about Z has a formula | It stops following; the status bar says so | |
