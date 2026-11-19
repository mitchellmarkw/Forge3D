# Test sheet: slice 52, Tolerance Test Print (0.61.0 Beta)

Time needed: about 10 minutes, plus a print (checks 6–7).

## Get the build

Open the pull request **Slice 52: Tolerance Test Print (0.61.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Tools → Tolerance Test Print...** | A window with "1. Make the test" (pegs, ball, pins with their sizes; fits from / to / every; "Holes labelled: .10 .15 .20 .25 .30") and "2. Record what fit best" (Printer label right on top of its box) | |
| 2 | Change the fits to 0.05 to 0.40 every 0.05; then every 0.01 | The labels list follows; with 0.01 it says that's more than 12 | |
| 3 | Back to 0.10–0.30 every 0.05, **Add to the Plate** | Three bars (peg holes, ball cups, pin holes), each hole with a raised label in front of it, readable from the front; two pegs, a ball on a stalk and two pins in a row in front. One Ctrl+Z takes them all away | |
| 4 | Add it again with something already on the plate | It goes beside it, not on top | |
| 5 | Untick the ball; Add | Just the peg and pin bars and their pieces | |
| 6 | Print it (resin, or FDM) | It prints; the labels are readable | |
| 7 | Try a peg in each hole, the ball in each cup, a pin in each pin hole | The fit gets looser hole by hole; note the one that's best for each | |
| 8 | Record them (Printer set to match) and **Use These Fits** | "Saved..." | |
| 9 | Select a shape, **Quick Socket**, choose that printer (switch away and back), then switch between Ball and Peg | The fit is the recorded one for each kind | |
| 10 | **Quick Hinge**, that printer | The fit is the recorded pin fit (a swivel: the peg fit) | |
| 11 | Split for Printing | The pegs' clearance starts at the recorded peg fit | |
| 12 | Close and reopen the app; open the window again | The recorded fits are still there | |
