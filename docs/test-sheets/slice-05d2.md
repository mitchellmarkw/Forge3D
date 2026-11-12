# Test sheet: slice 5d fixes (0.13.1)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 5d fixes: Emboss on balls and big models, stacked letters
(0.13.1)**, open its Windows build check, download the artifact, unzip and run
`Forge3D.exe`. **Help → About** says 0.13.1.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere, Emboss `Words on Sphere`, letter height 4, depth 1.5, Engraved (your screenshot) | The words run round the ball in an even band; every letter the same size and clean, including both ends | |
| 2 | Orbit round to each end of the words | No smeared or broken letters | |
| 3 | Import your big model (1.5M triangles), select it, click Emboss | The panel opens quickly; the words show in **orange** on the model, and the panel says it's a big shape | |
| 4 | Type new words, change size, turn, click elsewhere on the model | The orange words follow in about a second or less each time | |
| 5 | Click **Emboss** | A short wait (a few seconds at most), then the words are cut in / joined on | |
| 6 | Undo, Redo | Each takes a few seconds at most | |
| 7 | On the big model's result click **Change the words...**, change them, Apply | Orange preview again, quick; Apply makes the change | |
| 8 | Add a small Box (10 mm), Emboss a long line (e.g. `MUCH TOO LONG`, letter height 8) | A red message: the words are longer than the way round the shape | |
| 9 | Add → Text, tick **Stack the letters** | The word stands as a column, one letter under the other, each upright | |
| 10 | Type more letters | The column gets longer; the letters stay the same size | |
| 11 | Emboss on the side of a tall Box with **Stack the letters** ticked | A column of letters down the side | |
| 12 | Save, close and reopen | The stacked text and emboss come back stacked | |
| 13 | Repeat checks 9, 14, 17 and 24 of the slice 5d sheet | Same as before | |

## Send back

"All pass" or the failures, plus how long checks 4 and 5 take on your big model.
