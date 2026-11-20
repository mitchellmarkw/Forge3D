# Test sheet: slice 55, Print Check (0.64.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 55: Print Check (0.64.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Look at the ribbon on any tab | **Print Check** beside Measure in the Check group; greyed with nothing on the plate | |
| 2 | Add a Sphere and a Tube (R10 outside, R7 inside); click **Print Check** | The panel: Thin walls 0.80 mm, Overhangs 45°, the three colours. The sphere's underside is blue; the panel says "Sphere 1: needs supports under …"; the tube is all light grey (its bottom rests on the plate) | |
| 3 | Set **Thinner than** to 3.5 mm | The tube turns red; "Tube 1: too thin over … (thinnest about 2.98 mm)" | |
| 4 | Set it to 2.5 mm; untick **Overhangs** | The tube goes back to grey; the sphere's blue goes; with both unticked the panel says to tick one | |
| 5 | Set **Leaning out past** to 20° (Overhangs ticked) | More of the sphere is blue | |
| 6 | **Done**; turn the sphere 90° about X; Print Check again | It checks the parts the way they sit now | |
| 7 | Click in the empty view, then press **Esc** | Print Check closes (Esc works with nothing selected) | |
| 8 | Close and reopen Forge3D, Print Check | The thickness, angle and ticks are as you left them | |
| 9 | One of your sculpted figures (a big mesh): Print Check, thickness 0.8 | Overhangs show at once; "Measuring wall thickness…" for a moment (a few seconds on the slower machine), then thin fingers, cape edges or weapon tips show red. The app stays responsive meanwhile | |
| 10 | While it's open, Undo | The parts are checked again as they are now | |
