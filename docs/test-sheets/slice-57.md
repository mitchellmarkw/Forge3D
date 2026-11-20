# Test sheet: slice 57, Figure Guide (0.66.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 57: Figure Guide (0.66.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box. View tab → **Figure Guide** | The panel with **Add a Figure Guide** | |
| 2 | Add a Figure Guide | A see-through mannequin standing beside the box; the view moves to show it all. Figure size 6 in (152 mm), Realistic (7.5 heads), "A head is 20.3 mm tall" | |
| 3 | Measure its height (or look from the Front) | Feet on the plate, top of the head 152.4 mm up | |
| 4 | Figure size **3.75 in**; Proportions **Heroic (8 heads)**; then type a Height of 200 | It resizes each time; Figure size shows "Other height" for 200; the head size line follows | |
| 5 | Pose: **T-pose**, **Walking**, **Action** | It takes each pose and stays standing on the plate | |
| 6 | Joint: **Left elbow**, Bend 90; **Neck**, Turn 45 | Only Bend shows for the elbow; Nod and Turn for the neck; the joint moves; Pose shows "Your own" | |
| 7 | Turn 90, X 50 | It turns to face the right and moves along | |
| 8 | Undo a few times | Each step goes back (dragging a spin box is one step) | |
| 9 | Untick **Show the guide**; Done; Esc | It's hidden; the panel closes; it can be shown again from the panel | |
| 10 | Export STL; Print Check; click where it stands | It's not in the export, not checked, not clickable | |
| 11 | Save, close, open | The guide comes back as it was | |
| 12 | **Remove the Guide**, then Undo | It goes, then comes back | |
