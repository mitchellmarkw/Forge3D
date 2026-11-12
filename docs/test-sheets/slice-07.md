# Test sheet: slice 7, Sketches, Pad and Pocket (0.16.0)

Time needed: about 25 minutes.

## Get the build

Open the pull request **Slice 7: Sketches with dimensions and rules; Pad and Pocket
(0.16.0)**, open its Windows build check, download the artifact, unzip and run
`Forge3D.exe`. **Help → About** says 0.16.0.

## How sketching works (quick guide)

- **Sketch** button: with nothing selected you draw on the plate; with one shape
  selected, click the flat face to draw on.
- The 3D view is swapped for a flat drawing board, with a **Sketch** toolbar under the
  main one. The rest of the window is greyed out until you click **Finish** or
  **Cancel** in the panel on the right.
- Draw: **L**ine, **R**ectangle, **C**ircle, **A**rc, **S**lot. **Esc** stops what
  you're drawing, and a second Esc goes back to picking (**Pick**).
- Pick: click; **Ctrl+click** to pick more. Then click a rule (or press its key) or
  **D** for a dimension.
- Wheel zooms, right- or middle-drag pans, **F** fits.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, click **Sketch** | Flat grid with red / green axes through a grey dot (the origin); panel "Sketch on the plate"; Line tool on; every other button greyed | |
| 2 | Press **R**, click two corners | A rectangle with H and V labels on its sides; panel says it can still move 4 ways | |
| 3 | Esc, Esc, click the top side, press **D**, type 40, Enter. Click the right side, **D**, 30 | The rectangle becomes 40 × 30; "40 mm" and "30 mm" labels | |
| 4 | Click a corner, Ctrl+click the grey origin dot, **Shift+H**, 20. Again: corner + origin, **Shift+V**, 15 | The rectangle centres on the origin and turns **green**; panel: "Fully set" | |
| 5 | Double-click the "40 mm" label, type 50 | The rectangle grows to 50 wide and stays green | |
| 6 | Click the top side, press **V** | Refused: "That rule would shrink a line or circle to nothing…"; nothing changes | |
| 7 | Press **C**, click the origin, then a point 8 mm out. Esc, Esc. Pick the circle, **D**, 6 | A circle of radius 6 inside the rectangle (R6 label) | |
| 8 | **Ctrl+Z** a few times, then **Ctrl+Y** | Undo / Redo step through the sketch's own changes only | |
| 9 | Click **Finish** (Length 10) | Back in 3D: a 50 × 30 × 10 plate with a round hole through it, named Pad 1, sitting on the plate | |
| 10 | **Undo** in the main toolbar, then **Redo** | The whole pad goes, then comes back | |
| 11 | Select Pad 1, click **Sketch**, click its top face | Drawing board with the top face's outline dashed (and the hole) | |
| 12 | Press **S** (Slot): click the middle of one round end, then the other, then how wide. Esc, Esc. Drag a corner of the slot | It stays a slot (straight sides touching the round ends) as it changes | |
| 13 | In **Make:** (label directly above the list) pick **Pocket**, Depth 4, Finish | A slot-shaped pocket 4 mm deep in the top; Objects shows "Pocketed 1" with steps Pad 1 → Pocketed 1 | |
| 14 | Double-click the Pocketed step (or right-click → Change...) | The sketch opens again with the face outline; Make shows Pocket | |
| 15 | Tick **Through all**, Finish | The slot now goes right through. Undo puts it back to 4 mm | |
| 16 | Add a Box. Select it, **Sketch**, click its front face (the one facing you). Draw a circle to the right of the origin, Finish as Pad 5 | A peg sticking out of the front face, on the right side, as drawn | |
| 17 | Select the box with the peg, Sketch, click a curved face (if any) or the peg's round side | Status says the face is curved; nothing opens until a flat face is clicked; Esc cancels | |
| 18 | Sketch on the plate, **L**: click 3 points, then the first again | A closed triangle (the line tool stops when it closes) | |
| 19 | Draw an **Arc** (A: middle, start, end) without closing it, click Finish | Its two ends show red; Finish refuses: "The outline isn't closed…" | |
| 20 | Close it with a line from end to end (L, click each end), Finish | Pad made: a triangle with a D-shaped hole if the arc was inside it | |
| 21 | Pick two lines of the triangle, **Equal**; pick a line, **Helper** (G) | Equal: same length. Helper: the line turns dashed (and isn't part of the shape) | |
| 22 | Select a pad, drag a side handle with **Resize** | The pad scales (sideways evenly if it has circles); the height handle changes the length only | |
| 23 | Save, New, Open the file | Pads, pockets and their sketches come back; double-clicking still opens the sketch | |
| 24 | Sketch, then **Cancel** | Back to 3D; nothing added | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
