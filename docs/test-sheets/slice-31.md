# Test sheet: slice 31, Section view (0.40.0 Beta)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 31: Section view (0.40.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Make something with an inside: a Box with a Shell (or a hole that doesn't go through). **View → Section View** | A **Section View** panel under Objects; the model cut in half so you can see inside; the cut faces a flat darker shade | |
| 2 | Look at the panel | **Cut across:** with its label above the list; a slider, a position in mm, **Middle of the model**, **Show the other side** | |
| 3 | Drag the slider end to end | The cut moves smoothly through the model, from all of it showing to none | |
| 4 | Type a position; click **Middle of the model** | The cut goes exactly there; then back to the middle | |
| 5 | Tick **Show the other side** | The other half shows instead | |
| 6 | Choose **Front to back (Y)**, then **Bottom to top (Z)** | The cut goes through the other ways, starting from the middle each time | |
| 7 | With the view cut, orbit, then move the shape and change its size | Everything works as usual; the cut stays where it is on the plate | |
| 8 | Export an STL (or Save) with the view cut | The whole model is exported, not half | |
| 9 | Close the panel with its ✕ (or untick View → Section View) | The whole model shows again | |
| 10 | Open Section View with a sculpted or imported mesh | It cuts the same way | |

## Send back

"All pass" or the failures (with the number).
