# Test sheet: slice 51, Pose Test (0.60.0 Beta)

Time needed: about 25 minutes. Best with one of your figures that has Quick Hinge and
Quick Socket joints; checks 1–8 can be done with two cylinders.

## Get the build

Open the pull request **Slice 51: Pose Test (0.60.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Two cylinders, one on the other; Quick Hinge on the lower one's top, the other half made from the upper one, printed pin. Features → **Pose Test** | The panel lists "Hinge in Hinged 1 ↔ Hinged 2"; "Held still:" names one of them; "The build pose." | |
| 2 | Drag the part that isn't held | It turns round the pin, following the mouse; the angle shows in Bend; the pin moves with the held part | |
| 3 | Stop dragging | Within a moment: parts that hit tinted red, the overlap bright red, "Hit: … and …" in the panel. Straight: "Clear" or "The build pose" | |
| 4 | Type an angle; drag the slider | The part goes to that angle; the slider moves it smoothly | |
| 5 | **Find Range** with flat-ended cylinders | It says it hits almost straight away, and why (the ends are too close) | |
| 6 | Bevel or round the ends (or use a narrow upper part), Find Range again | "From straight it turns X° one way and Y° the other"; turning to just past X shows a hit | |
| 7 | Click the other part, **Hold Still** | That part stays put; the first one turns instead | |
| 8 | **Save Pose...** "Kneel"; **Back to the Build Pose**; choose Kneel in Pose; Delete; Ctrl+Z | Saved; straight; bent again; gone; back | |
| 9 | Save, close, open; Pose Test | The saved poses are still there | |
| 10 | **Done** (and Esc) | The parts show as built; nothing was changed (check their history and sizes) | |
| 11 | A figure with several joints (hip, knee, ankle; a ball-jointed shoulder) | Each joint listed; bending the knee carries the shin and foot; holding the torso, the limbs turn from it | |
| 12 | A ball joint: drag the limb, then type Bend / Bend sideways / Twist | It swings round in the plane you're looking at; the three angles follow; typing turns it exactly | |
| 13 | A double hinge (link and two pins) | Two joints; the link turns from the first fork, the other part from the link | |
| 14 | A big sculpted figure on your machine | Dragging stays smooth; the red check follows when you stop. Note roughly how long it takes | |
