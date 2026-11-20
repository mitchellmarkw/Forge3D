# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order to build them
(agreed 2026-11-19: joints and fit first, then print checks, scale, detailing, workflow).
Each has what's wanted, then notes on how it could fit Forge3D.

## 1. Thin-wall and overhang check

Find print problems before printing.

1. Shade the model where walls are thinner than a set thickness (0.8 mm, say): fingers,
   capes, weapon tips.
2. Shade faces that overhang past a set angle (45 degrees) when printed the way it
   sits on the plate, so they'll need supports.

Notes: a view mode like See-Through, using the mesh's shade (as the mask does).
Thickness: a ray inward from each point, against the same mesh.

## 2. Simplify a mesh

Bring a heavy sculpt down to a target size (number of triangles, or a percentage)
while keeping its shape, for lighter exports and a quicker app.

Notes: a mesh step (edge collapse with an error measure that keeps sharp edges and the
silhouette), undoable and changeable like Smooth.

## 3. Rescale, keeping joint sizes

Move a design to another scale (1:12 to 1:18, say) where the shapes scale but the
hardware doesn't.

1. Choose the new scale (or a new height).
2. Pegs, sockets, hinge pins, magnet pockets and the split pegs keep their real sizes;
   wall thickness can be kept too. Everything else scales.

Notes: the joint steps keep their placement but not their size when scaled
(`Scale.cpp` today scales them all).

## 4. Proportion guide (mannequin)

A simple posable stick figure to build to, like a 3D reference image.

1. Set by height in heads (realistic 7.5, heroic 8, stylised 6...) or by a figure
   line's height (3.75", 6", 7", or typed).
2. Pose its limbs; it's never printed or exported.

Notes: kept beside the reference images (not a body), drawn as simple shapes.

## 5. Hollow with drain holes

For resin printing: Shell plus drain and vent holes.

1. Pick where the holes go (or let Forge3D put them low and out of sight), and their
   size.
2. Optionally make plugs to close them after printing.

Notes: an option on Shell (holes through both walls), and plugs from Quick Socket's
peg cutters.

## 6. Surface stamps

Small details stamped onto a surface: rivets, bolts, panel vents, scales, stitching,
fabric seams.

1. A library of stamps (and your own, saved like custom shapes).
2. Click to stamp one, or drag to stamp a row along a line, at a size and spacing.

Notes: each stamp a small mesh merged onto the surface (raised) or cut in (sunk),
placed on the surface like Emboss.

## 7. Panel-line cutter

Draw a line on the surface and cut a groove of a set width and depth (for armour
plates, vehicle panels, scale models).

Notes: a path on the surface (as Follow draws one) swept with a small V or square
profile and cut away.

## 8. Split for painting

Cut along a colour boundary into pieces easier to paint, then glued back.

1. Split along painted regions (Split by Mask) or between colours already given.
2. Add small alignment keys on the cut so the pieces only go back one way.

Notes: Split by Mask plus the split pegs from Split for Printing.

## 9. Kit layout (sprue tree)

Lay out small parts (hands, accessories) on a frame like a model kit, joined by thin
gates, numbered.

Notes: an extension of Lay Out: a frame round the parts, gates to the nearest part
edges, embossed numbers.

## 10. Parts list

For every part: name, size, volume, and the estimated print weight and filament or
resin cost; printed or saved as a table (for planning, quoting, or a kit's
instructions).

Notes: volumes from the meshes; material density and price per kilo as preferences.
