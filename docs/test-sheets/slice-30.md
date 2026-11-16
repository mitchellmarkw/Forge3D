# Test sheet: slice 30, Text in sketches (0.39.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 30: Text in sketches (0.39.0)**, open its Windows build
check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Nothing selected, **Sketch**. Click **Text** on the sketch toolbar (or W), then click in the sketch | A small window: Words, **Font:** (its label above the list), Bold, Italic, Letter height, Turned | |
| 2 | Type your name, pick a font, height 12, OK | The words as letter outlines, starting at a small square point where you clicked | |
| 3 | Drag the words; then double-click them, tick Bold, Turned 20, OK | They move; then they're bold and tilted | |
| 4 | Double-click, Turned 0. **Finish** (Pad, 3 mm) | Letters standing 3 mm tall on the plate | |
| 5 | Double-click the pad; draw a rectangle right round the words. Finish | A plate with the letters cut right through it (a stencil); middles of letters like O and A are separate little pieces, as on any stencil | |
| 6 | Add a Box 60 × 30 × 10. Select it, **Sketch**, click its top. **Text**, click, type a word, height 8. **Make: Pocket**, Depth 1, Finish | The word engraved 1 mm into the top, with flat-bottomed letters | |
| 7 | Double-click the pocket step. Click the words' small square point, then **Across**; double-click the dimension and type a value. Finish | The engraving starts that far across from the grey origin dot | |
| 8 | Double-click the step, click the words, **Delete**, Finish | A message that there's no closed outline (nothing left to cut); Cancel | |
| 9 | Save, close, open; double-click a text shape | The same words, font and size, still changeable | |

## Send back

"All pass" or the failures (with the number).
