# Test sheet: slice 64, Tidy-up towards 1.0 (0.73.0 Beta)

Time needed: about 25 minutes (plus prints, if you do 12–13).

## Get the build

Open the pull request **Slice 64: Tidy-up towards 1.0 (0.73.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## The new help page

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Help** menu | **Figure, Toy and Kit Tools** under Quick Start Guide | |
| 2 | Open it; scroll through | A guide in sections (Starting a figure, Joints, Detail, Getting it printed...): each tool's name, where it is (e.g. "Features tab → Quick Hinge"), and what it does | |
| 3 | Check a few of the "where it is" lines against the app | Each tool is where the page says | |
| 4 | Dark theme (View → Theme → Dark), open it again | Readable: no dark text on a dark background | |
| 5 | **Help → Quick Start Guide** | A line near the end pointing to Help → Figure, Toy and Kit Tools | |

## Tools not yet tried by hand

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 6 | Two cylinders 1 mm apart, one above the other. Quick Hinge on the lower one's top: Joint **Double hinge**, Other half **Fork another part too**, pick the upper one, Apply | Both parts get a fork, facing each other; a "Hinge link" with two pin holes, and two pins, lined up | |
| 7 | Quick Hinge, Joint **Swivel** (Peg diameter 4, Peg goes in 5, Lip 0.4) on a cylinder's top, Other half a new peg shape, Apply. Then again, with **Made from another part** | A bore with a groove at the bottom; a "Swivel peg Ø4" with a lip and a slit across its end. The second time, the other part gets the peg on its end, standing just clear | |
| 8 | Features → **Pose Test** with the double hinge of 6 | Two joints listed; turning the first carries the link and the other part; the second turns the other part from the link | |
| 9 | Quick Socket (ball) between a "torso" and a "limb", then Pose Test; drag the limb, then type Bend / Bend sideways / Twist | It swings round in the plane you're looking at; the three angles follow; typing turns it exactly | |
| 10 | A figure with several joints (hip, knee, ankle, a ball-jointed shoulder), Pose Test | Each joint listed; bending the knee carries the shin and foot; holding the torso, the limbs turn from it | |
| 11 | Pose Test on one of your big sculpted figures | Dragging stays smooth; the red check follows when you stop. Note roughly how long it takes | |
| 12 | (Optional) print the double hinge of 6 | It bends further than a single hinge; both pins turn | |
| 13 | (Optional) print the swivel of 7 | The peg snaps in, turns, and doesn't pull out | |

## Speed on your slower machine

On one of your heaviest sculpts (or Simplify a copy to about a million triangles), time each of these roughly. Here (a quicker machine, 1.3 million triangles) each was 1–3 seconds.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 14 | **Print Check**, then change the thin-wall size (beside Thin walls) | The colours come within a few seconds; changing the setting recolours | |
| 15 | **Hollow** (2 mm walls, two drain holes), Apply | Done within several seconds | |
| 16 | **Stamp**: a row of rivets, Apply | Done within a few seconds | |
| 17 | **Panel Lines**: a long line round the shape, Apply | Done within a few seconds | |
| 18 | Anything that took much longer than 10 seconds or stuttered | Note which and how long, so it can be made faster | |

## Publishing 0.73 (optional, when you're happy)

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 19 | After merging: GitHub → Actions → the build workflow → **Run workflow** on main, **Publish** ticked | A release "Forge3D 0.73.0 Beta" with the Setup.exe and portable.zip, and the notes from docs/release-notes/v0.73.0.md | |
| 20 | Install the Setup.exe over 0.47 | It updates in place; .f3d files still open in Forge3D; Help → Check for Updates says it's up to date | |
