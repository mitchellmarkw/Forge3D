# Test sheet: slice 67, Joint Standards (0.76.0 Beta)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 67: Joint Standards (0.76.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Saving joints from the tools

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select a shape, Features → **Quick Socket** | At the top: "Joint standard:" (label right on top of its drop-down) showing "None in use", and **Save to Standard...** | |
| 2 | Set a ball socket (say 6 mm, grip 12 %, 4 ridges), click **Save to Standard...** | Asked to name a new standard (none in use yet): type "5-inch line"; then asked for the joint's name: "Shoulder ball" | |
| 3 | After saving | The label reads "From the joint standard '5-inch line':"; the status bar says it was saved | |
| 4 | Save a peg socket as "Wrist peg" and a magnet pocket as "Head magnet" | Both saved | |
| 5 | Save "Shoulder ball" again with different sizes | Asked whether to replace it; Yes replaces it | |
| 6 | Esc. **Quick Hinge** on a part: set a double hinge with clicks, Save to Standard as "Knee" | Saved; the hinge row lists only hinges ("Knee (…)") | |
| 7 | Open Quick Socket on another part: choose "Shoulder ball" from the list | The panel takes its sizes (kind, diameter, grip, ridges); the printer and fit stay as they were; the preview follows | |
| 8 | Set the printer to FDM, then choose "Wrist peg" | Peg sizes come over; still FDM with its fit | |
| 9 | Quick Hinge: choose "Knee" on a part, Apply | A double hinge with those sizes is made | |

## Managing them

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 10 | Tools → **Joint Standards...** | "Joint standard in use:" (label on top of its drop-down) showing 5-inch line; Notes; the joints with their kind and sizes | |
| 11 | Type notes; close and reopen the dialog | The notes are kept | |
| 12 | Rename Joint, Move Up / Move Down, Remove Joint | Each works; Quick Socket's list follows (reopen it) | |
| 13 | **Export...** to a file; **New...** "Mini line"; **Import...** the exported file | The import comes in as "5-inch line 2" and becomes the one in use | |
| 14 | Choose "None" as the standard in use | Quick Socket's row says "None in use" | |
| 15 | **Delete** a standard | Asked first; it's gone | |
| 16 | Close Forge3D and open it again | The standards and the one in use are still there (Documents\Forge3D\Joint Standards.json) | |

## The tolerance test

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 17 | With 5-inch line in use: Tools → **Tolerance Test Print** | A button "Use the Sizes of '5-inch line'" beside Add to the Plate | |
| 18 | Click it, then Add to the Plate | The peg, ball and pin sizes become the standard's (first ball, round peg, hinge pin); the test bars use them | |
| 19 | With no standard in use, open the Tolerance Test again | The button isn't there | |
| 20 | Help → Figure, Toy and Kit Tools | A Joint Standards section under Joints | |
