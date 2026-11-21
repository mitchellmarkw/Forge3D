# Test sheet: slice 66, Paint (0.75.0 Beta)

Time needed: about 30 minutes (plus a colour print, if you do 22).

## Get the build

Open the pull request **Slice 66: Paint (0.75.0)**, open its Windows build check, download the
`Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Painting

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Select one of your sculpted figures. Sculpt tab → **Paint** | The Paint panel: Painting with (a swatch and its name), 16 ready colours, Other Colour..., Brush size, Mirror left-right, Fill All, Undo Stroke, Clear. A ring shows the brush on the shape | |
| 2 | Click **Light skin**, then **Fill All** | The whole figure turns that colour; "1 stroke." | |
| 3 | Pick **Blue**, drag over a leg | The leg turns blue where the ring passed, with a clean edge | |
| 4 | Tick **Mirror left-right**, paint a boot | The other boot is painted too | |
| 5 | Hold **Ctrl** and drag over some paint | It goes back to the shape's own colour (not the skin) | |
| 6 | **[** and **]** | The brush (and ring) gets smaller and bigger | |
| 7 | **Undo Stroke**, then Ctrl+Z | The last strokes come off, one at a time | |
| 8 | **Other Colour...**, choose a colour, paint with it | It paints in that colour; "On this shape:" lists the colours used, to pick again | |
| 9 | **Apply** | One step "Painted 1 · N colours"; the figure shows its colours | |
| 10 | Ctrl+Z, Ctrl+Y | The colours go, then come back | |
| 11 | Select it and click **Paint** again | "Change the paint on '…'": the same step, its strokes there; paint more, Apply: one undo step "Change paint" | |
| 12 | Double-click the Painted step in Objects | Opens the same way | |
| 13 | Add a **Box** and paint a spot on one face | Just the spot is painted (not the whole face) | |
| 14 | Move, turn and resize the painted figure; Rescale Design ×2 | The paint stays on it, in the same places | |
| 15 | Save, close, open | The colours are there | |
| 16 | Change the figure's own colour (Properties) | Unpainted places take the new colour; painted ones keep theirs | |
| 17 | On a heavy sculpt: Paint, Fill All, paint strokes | Note how long Paint takes to open and whether painting keeps up with the mouse | |

## Splitting along a colour

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 18 | Painted figure: Design tab → **Split by Mask** | "Mask from a painted colour:" (label right on top of its drop-down), listing the colours painted, each with a swatch | |
| 19 | Choose the boots' colour | The mask (dark) covers exactly where that colour is | |
| 20 | Apply (Both pieces, keys on) | Two pieces, cut along the colour's edge; both keep their colours | |

## Exporting for a colour printer

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 21 | Export the painted figure as **3MF**; open it in your slicer (Bambu Studio, OrcaSlicer or PrusaSlicer) | The figure opens painted: its own colour as filament 1, each painted colour as the next filament (set the filament colours to match) | |
| 22 | (Optional) Print it on a multi-colour printer | The colours land where they were painted | |
| 23 | Export a plain (unpainted) shape as 3MF | Opens as before, in its colour | |
| 24 | Help → Figure, Toy and Kit Tools | A Paint section; Split by Mask mentions the painted colours | |

Note which slicer you tried in 21, and anything it said when opening the file.
