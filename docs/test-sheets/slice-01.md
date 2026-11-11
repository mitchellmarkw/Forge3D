# Test sheet: slice 1 (skeleton, viewport, navigation)

Time needed: about 10 minutes, once the build is downloaded.

## Get the build

1. On GitHub, open **Actions → Windows build** and click the newest run for this branch.
   The first run takes 1–3 hours (it compiles Qt and OpenCASCADE once); later runs are
   much faster.
2. If the run is green, download the artifact `Forge3D-windows-x64-…` at the bottom of
   the run page and unzip it.
3. If the run is red, open the failed step, copy the last ~50 lines of its log, and send
   them back. That is the whole report for this case.

## Checks

Mark each one Pass or Fail. For a Fail, add a sentence or a screenshot.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Double-click `Forge3D.exe` | A window titled "Forge3D 0.1.0" opens within a few seconds, with no error dialog | |
| 2 | Look at the view | A grey grid (the build plate) with a red line (+X) and a green line (+Y), and a blue box sitting on the grid | |
| 3 | Look at the box | The top, front and side faces have clearly different shades; edges look straight and crisp | |
| 4 | Look at the status bar | Left: the mouse hints. Right: "Box 40 x 30 x 20 mm \| 12 triangles" and an OpenGL version (3.3 or higher) | |
| 5 | Hold the right mouse button and drag | The view orbits around the box; dragging up/down tilts, but the view never flips upside down | |
| 6 | Hold the middle button (wheel) and drag | The view pans; the box follows the mouse | |
| 7 | Hold Shift + right button and drag | Same as step 6 | |
| 8 | Scroll the wheel | Zooms in and out smoothly toward the centre of the view | |
| 9 | Press **F** | The box is centred and fills the view | |
| 10 | Orbit somewhere odd, then press **Home** | The view returns to the starting angle | |
| 11 | Resize the window, then maximise it | The view redraws correctly with no stretching | |
| 12 | Drag the window to a second monitor (if you have one) | Still draws correctly | |
| 13 | *Help → About Forge3D* | Shows versions of Forge3D, Qt, OpenCASCADE and OpenGL | |
| 14 | *Help → Open log folder* | Explorer opens a folder containing `forge3d.log` | |
| 15 | *File → Exit* | The app closes with no error | |

## Send back

- The results table (or just "all pass" plus any failures).
- `forge3d.log` from step 14 — useful even when everything passes, because it records
  your graphics card and driver.
- If the app crashed: the newest `.dmp` file from `%LOCALAPPDATA%\Forge3D\crashes`.
