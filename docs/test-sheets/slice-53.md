# Test sheet: slice 53, Clicks and ridges (0.62.0 Beta)

Time needed: about 15 minutes, plus printing checks 9–11 when you can.

## Get the build

Open the pull request **Slice 53: Clicks and ridges (0.62.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Quick Hinge on a cylinder's top: Clicks a turn **8**, Click depth 0.4 | In blue, the tab with V ridges on both faces round the pin; Click depth was greyed until Clicks was set | |
| 2 | Clicks 36 on a 5 mm tab end; Click depth 0.1 with fit 0.1 | Each says what's wrong (too many clicks for the size; deeper than the fit) and Apply is off | |
| 3 | Apply (new tab shape). Look at the fork's arms (Section View helps) | Grooves in the arms' inner faces round the pin hole | |
| 4 | Double-click the step: History/Objects | It says "…, 8 clicks"; change to 12 and Apply: the tab half (if made from another part) changes too | |
| 5 | Double hinge with clicks | The link has ridges at both ends; both forks grooves | |
| 6 | Swivel with 8 clicks | Ridges along the peg, grooves along the bore | |
| 7 | Quick Socket, ball: **Ridges** 6, Ridge height 0.2 | Ribs left inside the cup (look with Section View); the history says "6 ridges". For a peg the ridge settings aren't shown | |
| 8 | Pose Test on the clicky hinge: drag, type 30 | It snaps to 0, 45, 90…; typing 30 rests at 45. Find Range gives a multiple of 45 | |
| 9 | Print a clicky hinge (resin or FDM) | It clicks round and holds each position | |
| 10 | Print the swivel with clicks | It clicks round | |
| 11 | Print a ball socket with ridges | Stiffer than one without | |
| 12 | Save, close, open | Clicks and ridges are still there | |
