# Test sheet: slice 5d (Text and Emboss)

Time needed: about 20 minutes.

## Get the build

Open the pull request **Slice 5d: Text and Emboss (0.13.0)**, open its Windows build
check, download the artifact, unzip and run `Forge3D.exe`. **Help → About** says 0.13.0.

## Checks

### The Text shape

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Add → Text** | The word "Text" in 3D, lying flat, letters about 10 mm tall, in Arial. Properties has a Text box: Words, Font (label right above the list), Bold, Italic, Letter height | |
| 2 | Type `Hello World` in Words and press Enter | The letters change; the block gets longer, the letters stay the same size. One Undo goes back | |
| 3 | Open the Font list and pick another font (e.g. Comic Sans MS or Impact) | The list shows your installed fonts; the letters change to the one you pick | |
| 4 | Tick Bold, then Italic | Thicker, then slanted letters | |
| 5 | Set Letter height to 20 | Twice as big; Width and Depth double too | |
| 6 | Change Width only (Uniform off) | The letters stretch sideways | |
| 7 | Empty the Words box and press Enter | Nothing changes; the old words come back in the box | |
| 8 | Merge the text with a box under it; Export as 3MF and open it in your slicer | A name plate; no repair warnings | |

### Emboss

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 9 | Add a Box. Select it, click **Emboss** | The panel on the right; the word "Text" raised on the side facing you, at once | |
| 10 | Type your name in Words | After a moment the view shows your name | |
| 11 | Click the top of the box | The words move to the top | |
| 12 | Change Letter height, Depth and Turn | The view follows each change | |
| 13 | Choose **Engraved** | The words are cut in instead | |
| 14 | Click **Emboss** | Done: Properties says "Made from: Box 1, with the words ... engraved". Undo / Redo work | |
| 15 | Click **Change the words...**, change them, click Apply | The words change in place | |
| 16 | Click **Change the words...**, then Cancel (or Esc) | Nothing changes | |
| 17 | Add a Cylinder; Emboss a long word (e.g. `WRAPPED`, letter height 7) on its side | The letters wrap round the curve, each the same distance out; none sticks out flat | |
| 18 | Emboss words on a Sphere | They bend over the ball | |
| 19 | Make the letters far too big for the shape | A red message in the panel: they would hang off the edge. The Emboss button is greyed out | |
| 20 | Emboss words on your big imported model (the 1.5M-triangle one) | Works; note how long each preview takes | |
| 21 | Select an emboss result, click **Remove the words** | The plain shape is back | |
| 22 | Resize an emboss result with Uniform on | The words grow with it | |
| 23 | Save, close, reopen a file with text and embossed shapes | All come back the same, still editable | |
| 24 | Export an embossed shape as 3MF; open it in the slicer | No repair warnings | |

## Send back

"All pass" or the failures (screenshots help for anything that looks wrong), plus the
preview time for check 20, and `forge3d.log` if anything failed.
