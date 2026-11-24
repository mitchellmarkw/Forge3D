# Test sheet: 0.100.0, Slice 92: Rotate for Best Print, for resin too

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 92: Rotate for Best Print, for resin too (0.100.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 40 × 30 × 4 (a flat plate); Tools → Rotate for Best Print | **Printer:** (label above the list) and **Best for:**, a **Turn it to take the least room** box (ticked), and a **Biggest layer** row | |
| 2 | Printer Filament, Best Quality | Already the best way (lying flat) | |
| 3 | Printer **Resin**, Best Quality | Turned: Biggest layer drops from about 1200 mm² to a small fraction; the description talks about the film | |
| 4 | Resin, Fastest Print | Lying flat (the lowest) is best | |
| 5 | Rotate with Resin, Best Quality; then Undo | The plate stands up or tips; Undo lays it flat again | |
| 6 | Add a Box 10 × 40 × 6; turn it 30° about Z (Properties, Around Z) | | |
| 7 | Rotate for Best Print, Filament, Fastest, Turn box ticked; Rotate | It lies flat, square to the plate, its 40 mm side left to right | |
| 8 | Undo; untick Turn; Rotate | Still at 30° on the plate | |
| 9 | A figure part (an arm or a torso); try Resin, Best Quality | It's tipped, not lying flat or standing straight up | |
| 10 | Close and open Forge3D; Rotate for Best Print | Resin and the Turn box as you left them | |
