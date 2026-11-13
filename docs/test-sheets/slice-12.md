# Test sheet: slice 12, Emboss a picture (0.21.0)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 12: Emboss a picture (0.21.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says
0.21.0. The sample pictures are in the zip's `test-sheets\samples` folder:
`forge-logo.svg` (a ring with a star), `square-logo.png` (a rounded square with a hole)
and `shaded-photo.jpg` (a soft shaded picture). Any logo or photo of your own works too.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box (make it 40 × 40 × 20). Select it, Design → **Emboss** | The Emboss panel; at the top, label "Emboss:" with the box "Words" right under it | |
| 2 | Emboss: **A picture (logo or photo)** | The words part is replaced by a Picture part: Choose a picture..., "No picture yet.", Kind (label above): Logo (one height), the "Where dark ends" slider at 128, Invert, Width. The line at the top says to choose a picture; the Emboss button is greyed | |
| 3 | **Choose a picture...** → `forge-logo.svg` | "forge-logo.svg (1000 × 1000)"; the preview shows the ring and star raised on the side facing you, about half the box's width | |
| 4 | Width 30, Depth 2, Turn 45 | The preview follows each change | |
| 5 | Choose `square-logo.png` | The rounded square with its round hole, and the small square inside the hole with its own little hole | |
| 6 | Slide **Where dark ends** to the far left and let go, then to the far right | The outline shrinks a little (left) and grows a little (right) once you let go: this picture is only black and white, so it changes little. Back to the middle | |
| 7 | Tick **Invert** | The white parts become the shape instead: a frame round the outside, and the round hole as a disc (with a square hole in it) | |
| 8 | Untick Invert. **Engraved**, then click the top of the box | The logo cut into the top | |
| 9 | **Center on the face → All** | It moves to the middle of the top | |
| 10 | **Emboss** | Objects: "Emboss 1  picture square-logo.png engraved"; Properties: "Made from: Box 1, with the picture square-logo.png engraved", buttons **Remove the picture** and **Change the picture...** | |
| 11 | **Change the picture...** | The panel opens titled "Change the picture on 'Emboss 1'", the Emboss box greyed (it stays a picture), its settings as you left them | |
| 12 | Kind: **Photo (shades become heights)**, choose `shaded-photo.jpg`, Raised, Apply | The slider is gone (Invert now reads "light parts stand highest"); a raised relief: the dark corners standing up most, the light ring lowest, the grey middle in between | |
| 13 | Undo / Redo | One step each | |
| 14 | Add a Cylinder (R 15, 40 tall). Emboss → picture, `forge-logo.svg`, Width 20, click the curved side | The logo wraps round the curve and stands out evenly all over | |
| 15 | Width 120 | "The picture is wider than the way round the shape…"; Emboss greyed | |
| 16 | Width 20, Turn 90 | The logo turns to run up the side | |
| 17 | Resize the whole Emboss result evenly to twice the size (lock proportions) | The logo grows with it | |
| 18 | Emboss the box again with **Words** | Words work as before; the panel remembers Words was used last, and next time a picture if that was last | |
| 19 | Save, New, Open | Both embossed shapes come back as they were, and Change the picture... still works (the picture is inside the file) | |
| 20 | Export the cylinder as STL and open it in your slicer | A clean, watertight model with the logo | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
