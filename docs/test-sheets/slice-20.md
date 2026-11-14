# Test sheet: slice 20, view cube (0.29.0 Beta)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 20: View cube (0.29.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Start Forge3D, add a Box | A cube in the top-right corner of the view showing Front, Right and Top, with three round buttons under it | |
| 2 | Turn the view with the right button | The cube turns with it; the side facing you is the side of the model facing you | |
| 3 | Point at the middle of "Front" | That piece lights up blue; the pointer becomes a hand | |
| 4 | Click it | The view turns smoothly to look straight at the front | |
| 5 | Click Top, Right, Back, Left, Bottom in turn (turn the view to reach them, or use the buttons) | Each turns the view to that side; Top has Front at the bottom of the screen | |
| 6 | Click an edge piece (between two labels) and a corner piece | A 45° view between the two sides; a three-quarter view from the corner | |
| 7 | The left and right round buttons | The view turns 90° left / right | |
| 8 | The middle (house) button | Back to the starting view | |
| 9 | Clicking the cube doesn't select or move anything in the scene behind it | Nothing selected or moved | |
| 10 | View → **View Cube** off, restart, then on | Hidden, stays hidden after a restart, comes back when ticked | |

## Send back

"All pass" or the failures (with the number).
