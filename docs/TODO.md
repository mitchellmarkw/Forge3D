# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order to build them
(agreed 2026-11-20: posing a sculpt, then painting colours; the tidy-up towards 1.0 is slice 64).
Each has what's wanted, then notes on how it could fit Forge3D.

## 1. Posing a sculpted figure

Put simple bones inside one sculpted figure and bend it at the joints, the surface
following smoothly (for a figure sculpted in one piece, or to try a pose before cutting
joints).

Notes: the figure guide's skeleton (heads, joints, angles) to start the bones from;
each vertex weighted to its nearest bones (smooth falloff at joints); a step in the
history so the pose can be changed or undone; the rest pose kept.

## 2. Painting colours onto one shape

Brush colours onto areas of a single figure (skin, clothes, eyes), see them in the view,
and export a multi-colour 3MF for colour printers.

1. A colour brush (like the mask brush) with a palette.
2. Colours kept with the shape and saved in the file.
3. 3MF export with the colours (per triangle); Split for painting can split along them.
