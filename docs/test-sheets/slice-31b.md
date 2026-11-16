# Test sheet: slice 31+, Section view for one shape (0.40.1 Beta)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Section view for one shape (0.40.1)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Put three shapes on the plate. **View → Section View** | The panel shows all of itself (nothing squeezed off the bottom); a new **Cut:** list at the top, its label above it, on **Everything on the plate**; all three shapes cut, as before | |
| 2 | Choose **Only the selected shape** with nothing selected | Nothing is cut yet | |
| 3 | Click one shape | Only that shape is cut open; the other two stay whole | |
| 4 | Drag the slider | It travels across just that shape, end to end | |
| 5 | Click another shape | That one is cut instead; the first is whole again | |
| 6 | Click empty plate (nothing selected) | The last shape stays cut | |
| 7 | Ctrl+click two shapes | Both are cut | |
| 8 | Back to **Everything on the plate** | All three cut again | |
| 9 | Close the panel | Everything whole | |

## Send back

"All pass" or the failures (with the number).
