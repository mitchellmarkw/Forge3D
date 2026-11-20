# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order to build them
(agreed 2026-11-19: joints and fit first, then print checks, scale, detailing, workflow).
Each has what's wanted, then notes on how it could fit Forge3D.

## 1. Hollow with drain holes

For resin printing: Shell plus drain and vent holes.

1. Pick where the holes go (or let Forge3D put them low and out of sight), and their
   size.
2. Optionally make plugs to close them after printing.

Notes: an option on Shell (holes through both walls), and plugs from Quick Socket's
peg cutters.

## 2. Surface stamps

Small details stamped onto a surface: rivets, bolts, panel vents, scales, stitching,
fabric seams.

1. A library of stamps (and your own, saved like custom shapes).
2. Click to stamp one, or drag to stamp a row along a line, at a size and spacing.

Notes: each stamp a small mesh merged onto the surface (raised) or cut in (sunk),
placed on the surface like Emboss.

## 3. Panel-line cutter

Draw a line on the surface and cut a groove of a set width and depth (for armour
plates, vehicle panels, scale models).

Notes: a path on the surface (as Follow draws one) swept with a small V or square
profile and cut away.

## 4. Split for painting

Cut along a colour boundary into pieces easier to paint, then glued back.

1. Split along painted regions (Split by Mask) or between colours already given.
2. Add small alignment keys on the cut so the pieces only go back one way.

Notes: Split by Mask plus the split pegs from Split for Printing.

## 5. Kit layout (sprue tree)

Lay out small parts (hands, accessories) on a frame like a model kit, joined by thin
gates, numbered.

Notes: an extension of Lay Out: a frame round the parts, gates to the nearest part
edges, embossed numbers.

## 6. Parts list

For every part: name, size, volume, and the estimated print weight and filament or
resin cost; printed or saved as a table (for planning, quoting, or a kit's
instructions).

Notes: volumes from the meshes; material density and price per kilo as preferences.
