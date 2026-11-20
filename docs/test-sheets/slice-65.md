# Test sheet: slice 65, Pose Figure (0.74.0 Beta)

Time needed: about 30 minutes.

## Get the build

Open the pull request **Slice 65: Pose Figure (0.74.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

You'll need a figure sculpted in one piece (or an imported STL of one), standing straight and facing the front.
Arms out (an A or T pose) works best; try one with arms down too.

## Fitting the skeleton

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select the figure. Sculpt tab → **Pose Figure** | The panel "Pose '…'": 1. Fit the Skeleton (pressed) and 2. Pose. The figure goes see-through with an orange skeleton in it: balls at the head, neck, waist, shoulders, elbows, wrists, hands, hips, knees, ankles and toes | |
| 2 | Look at where the balls sit (front view, then side view) | Roughly in the middle of each joint; the arms' balls run along the arms whether they're out or down | |
| 3 | With **Move both sides together** ticked, drag the left knee ball | It follows the mouse (in the plane facing you); the right knee moves the same way, mirrored | |
| 4 | Untick it and drag one elbow | Only that one moves | |
| 5 | **Place It Again** | The skeleton goes back to where it was first placed | |
| 6 | Change **Bends spread** | Nothing changes yet (it's used when posing) | |

## Posing

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 7 | Click **2. Pose** | The figure shows solid again, as it was made (nothing moved). Pose: "As it was made" | |
| 8 | Pose: **Standing**, then **Action**, then **Walking** | The figure takes each pose; arms and legs bend smoothly at the joints; it stays standing on the plate | |
| 9 | Joint: **Left elbow** (label right on top of its drop-down), type Bend 90 | The left forearm bends forward; the elbow keeps its thickness (no pinched or collapsed elbow) | |
| 10 | Drag a hand ball, then a knee ball, then the head ball | The joint above each turns to follow the mouse (shoulder... hip, neck); the Joint drop-down and its angles follow | |
| 11 | Drag a shoulder or hip ball | A message: it doesn't bend anything; drag a ball further along | |
| 12 | Bend both knees a lot (Action, or type Hip Forward 45 and Knee 90) | The figure comes down to stand on the plate; the feet stay flat | |
| 13 | Untick **Feet flat on the ground** | The feet follow the shins; the ankles' angles can now be typed (Point toes) | |
| 14 | Untick **Keep it standing on the plate** | Bent legs leave it where the hips were (floating) | |
| 15 | Change Bends spread (back on 1. Fit the Skeleton), then 2. Pose | Larger: softer, rounder bends; smaller: tighter at the joint | |
| 16 | With a Figure Guide in the file (View → Figure Guide), open Pose Figure | Pose has "The Figure Guide's pose" too; choosing it copies the guide's pose | |

## In the history

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 17 | **Apply** | One step "Posed 1 · posed, N joints moved, feet flat"; the posed figure | |
| 18 | Ctrl+Z, then Ctrl+Y | The figure as it was; posed again | |
| 19 | Double-click the step in Objects | The panel opens on 2. Pose with the pose as it was; change something, Apply: one undo step "Change pose" | |
| 20 | **Sculpt** the posed figure, Done | Sculpting works on the posed figure (a Sculpt step on top) | |
| 21 | Double-click the original sculpt step under the pose, add a stroke | The pose is made again with the change | |
| 22 | Save, close, open | The step and its pose are there; double-click still opens it | |
| 23 | Tools → **Rescale Design** ×2 | The posed figure grows; double-click the step: the skeleton fits the bigger figure | |
| 24 | Print Check / Hollow / Split by Mask on the posed figure | They work as on any sculpt | |
| 25 | One of your heavy sculpts: open Pose Figure, go to 2. Pose, drag a hand | Note roughly how long 2. Pose takes to show, and whether dragging stays smooth | |
| 26 | Help → Figure, Toy and Kit Tools | A Pose Figure section under Starting a figure | |
