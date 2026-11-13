# Test sheet: slice 13, Phase 1 polish (0.22.0)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 13: Phase 1 polish (0.22.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.22.0.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Start Forge3D (or File → New) | A note over the view: "Start by adding a shape: Build → Add Shape (or the Add menu). Or Import an STL, 3MF or OBJ file, or drop one here." | |
| 2 | Add a Box | The note goes. Clear Plate brings it back; Undo hides it again | |
| 3 | Look at the Build tab's Undo button, then point at it | It just says "Undo"; pointing at it shows "Undo Add Box" (Edit menu still shows the full name) | |
| 4 | Make the window narrower (drag its edge) until the Build buttons don't fit | The buttons keep their names; a scroll bar appears under the row to reach the rest. Widen it again: the scroll bar goes | |
| 5 | Add a Wedge. Print tab → **Lay on Face** | The button stays pressed; the handles go; the status bar says to click a face | |
| 6 | Click the wedge's sloping face | The wedge turns so the slope lies flat on the plate, its square corner up; still where it was across the plate. Lay on Face pops back up | |
| 7 | Undo | The wedge back as it was, in one step ("Undo Lay on Face") | |
| 8 | Lay on Face, then press Esc | Nothing changes; the button pops up; the handles come back | |
| 9 | Lay on Face on the Box's side face | The box turns onto that side, resting on the plate | |
| 10 | Lift each shape (select it, Z 10 in Properties), then select both (Ctrl+click) and Print → **Sit on Plate** | Both drop to the plate together; one Undo puts both back up | |
| 11 | Make an Emboss (or any step) in a shape, click elsewhere so it isn't selected, then double-click its step row in Objects | The step's tool opens straight away, every time (try a few times) | |
| 12 | Merge two shapes; double-click the Merge row | It opens / closes to show its parts (nothing to change there) | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
