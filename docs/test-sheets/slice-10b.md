# Test sheet: 0.19.1, Follow no longer hangs off the edge

Time needed: about 5 minutes.

Download the Windows build artifact of PR **Follow: no hang when the path runs off the edge (0.19.1)**. **Help → About** says 0.19.1.

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Box 20 × 20 × 10, Follow, click its top, draw a circle bigger than the box, Done | Straight away: "The path runs off the edge of the surface it's drawn on…"; Apply greyed; nothing freezes | |
| 2 | Make the circle smaller than the top, Done, Apply | The ring groove, as before | |
| 3 | Make the Box step smaller than the ring (e.g. 8 × 8) | No freeze: the box shows without the groove, the Grooved step marked ⚠; hovering it gives the reason | |
| 4 | Undo | The groove is back | |
| 5 | Drag the box's resize handle smaller past the ring, and back | No freeze; the groove disappears (⚠) and comes back | |
| 6 | Round a box's edges (Edges → Rounding 3), then Follow on its top with a line that runs over the rounded edge onto the side | The groove follows over the rounding, or stops with the message where the side turns, without freezing | |
| 7 | A path round a cylinder (as in slice 10) | Still works | |

Send back "All pass" or the failures.
