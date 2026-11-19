# Test sheet: 0.59.1, Scale Calculator and mask colours

Time needed: about 10 minutes.

## Get the build

Open the pull request **0.59.1: Scale Calculator and mask colours**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Menu bar: **Tools → Scale Calculator...** | A small window: Real size and Unit, Scale "1 : …" and Answer in (each label right on top of its box), and the answer in big letters. No new ribbon tab | |
| 2 | Real size 6, Unit **feet**, Scale 60, Answer in **mm** | "= 30.48 mm on the model" | |
| 3 | Try each unit: 1 yard at 1:1 in mm; 10 cm at 1:2 in mm; 2 m at 1:12 in inches | 914.40 mm; 50.00 mm; 6.562 inches | |
| 4 | **Copy**, then paste into a shape's size box | The number pastes in | |
| 5 | Leave it open and work in the main window; close it and open it again; restart the app | It stays beside the window; it comes back with what you typed | |
| 6 | Select a shape, **Split by Mask**, paint on it | Yellow shape, black paint, evenly lit: no shadows that look like paint | |
| 7 | **Colours while painting**: click Not painted and pick white; **Swap**; **Reset** | The shape changes colour straight away; Swap swaps them; Reset gives yellow and black again | |
| 8 | Cancel, start Split by Mask again (and restart the app) | Your colours are kept | |
| 9 | Sculpt a shape; choose the **Mask** brush, then another brush | With Mask, the same plain colours; with the others, the usual look (mask shown dark) | |
| 10 | Reference Images → Size from two points: the Unit list | Has yards too | |
