# Test sheet: slice 4a2 (drag the Split plane)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 4a2: drag the Split plane (0.7.1)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.7.1.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, select it, click **Split** | The yellow plane through its middle | |
| 2 | Drag the yellow plane up and down | It slides through the box in 1 mm steps; the slider and "From the middle" follow; the status bar shows the position | |
| 3 | Hold **Alt** while dragging | It moves smoothly, without steps | |
| 4 | Press **Across X**, orbit so the plane is seen edge-on, drag its arrow | The plane slides left and right | |
| 5 | Start a drag, press **Esc** before letting go | The plane jumps back to where the drag started; the tool stays open | |
| 6 | Click on empty space in the view | Nothing happens: the tool stays open, the box stays selected | |
| 7 | Click **Split** (Keep both) | "Box 1 (top)" is selected on its own with move arrows. Press **E**: rotate rings; **W**: arrows again | |
| 8 | Click the bottom half | It gets the handles instead | |

## Send back

"All pass" or the failures.
