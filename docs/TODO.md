# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order to build them
(agreed 2026-11-19: joints and fit first, then print checks, scale, detailing, workflow).
Each has what's wanted, then notes on how it could fit Forge3D.

## 1. Quick Hinge

Hinge and swivel joints, the way Quick Socket does balls and pegs.

1. **Hinge**: a barrel on one part and a fork (clevis) on the other, both cut at
   once, with a pin hole through them. Single or double hinge (knees, elbows).
2. **Swivel**: a round peg with a retaining lip in a matching bore (waist, wrists,
   biceps cuts).
3. **Pin**: sized for a printed pin, a length of filament (1.75 mm) or a wire or rod
   of a typed diameter, with the printer's clearance added.

Notes: a new mesh step like `BooleanOp::Socket`, placed by picking a face (as Quick
Socket does), with the clearance from the printer setting (or from the tolerance test,
item 3). Join Parts should recognise the hinge halves.

## 2. Pose test

Check how far a joint moves before printing.

1. Mark a joint's pivot (a Quick Socket or Quick Hinge already knows it; otherwise
   click a ball or pick an axis).
2. Rotate the part (and anything joined to it) round the pivot by dragging or typing
   an angle; parts that hit are shown in red, and the range before they hit is shown.
3. Save poses by name, to show off or check again; going back to the build pose is
   one click. Poses never change the parts as printed.

Notes: needs a parent/child link between joined parts (Join's "keep together" groups
are a start). Collisions: a mesh-against-mesh overlap test, kept quick by checking only
the parts near the joint.

## 3. Tolerance test print

Find the fit that suits your printer, once.

1. Make a small strip of pegs and holes (and balls and cups, and hinge pins) at stepped
   clearances: 0.10, 0.15, 0.20, 0.25, 0.30 mm, each labelled.
2. Print it, try them, and pick the one that fits best for each kind (snug for pegs,
   friction for balls).
3. That becomes the default clearance for Quick Socket, Quick Hinge, Join and the
   split pegs; it can be changed later.

Notes: built from the socket cutters with embossed numbers; the chosen clearances kept
as preferences per kind of joint.

## 4. Ratchet and click joints

Joints that hold a pose.

1. Teeth on a hinge's faces or round a swivel, with how many teeth (clicks per turn)
   and how deep.
2. A ball-and-cup option with ridges for a stiffer hold.

Notes: an option on Quick Hinge and Quick Socket rather than a new tool.

## 5. Magnet pockets

Swappable heads, hands and accessories.

1. Pick a spot: a pocket sized for a standard magnet (6x2, 5x1, 3x1, 3x2 mm, or typed),
   plus clearance, at a depth to sit flush or under a thin skin.
2. Optionally the matching pocket on the mating part, lined up (as Join Parts does).

Notes: another Quick Socket kind with a plain cylinder cutter.

## 6. Thin-wall and overhang check

Find print problems before printing.

1. Shade the model where walls are thinner than a set thickness (0.8 mm, say): fingers,
   capes, weapon tips.
2. Shade faces that overhang past a set angle (45 degrees) when printed the way it
   sits on the plate, so they'll need supports.

Notes: a view mode like See-Through, using the mesh's shade (as the mask does).
Thickness: a ray inward from each point, against the same mesh.

## 7. Simplify a mesh

Bring a heavy sculpt down to a target size (number of triangles, or a percentage)
while keeping its shape, for lighter exports and a quicker app.

Notes: a mesh step (edge collapse with an error measure that keeps sharp edges and the
silhouette), undoable and changeable like Smooth.

## 8. Rescale, keeping joint sizes

Move a design to another scale (1:12 to 1:18, say) where the shapes scale but the
hardware doesn't.

1. Choose the new scale (or a new height).
2. Pegs, sockets, hinge pins, magnet pockets and the split pegs keep their real sizes;
   wall thickness can be kept too. Everything else scales.

Notes: the joint steps keep their placement but not their size when scaled
(`Scale.cpp` today scales them all).

## 9. Proportion guide (mannequin)

A simple posable stick figure to build to, like a 3D reference image.

1. Set by height in heads (realistic 7.5, heroic 8, stylised 6...) or by a figure
   line's height (3.75", 6", 7", or typed).
2. Pose its limbs; it's never printed or exported.

Notes: kept beside the reference images (not a body), drawn as simple shapes.

## 10. Hollow with drain holes

For resin printing: Shell plus drain and vent holes.

1. Pick where the holes go (or let Forge3D put them low and out of sight), and their
   size.
2. Optionally make plugs to close them after printing.

Notes: an option on Shell (holes through both walls), and plugs from Quick Socket's
peg cutters.

## 11. Surface stamps

Small details stamped onto a surface: rivets, bolts, panel vents, scales, stitching,
fabric seams.

1. A library of stamps (and your own, saved like custom shapes).
2. Click to stamp one, or drag to stamp a row along a line, at a size and spacing.

Notes: each stamp a small mesh merged onto the surface (raised) or cut in (sunk),
placed on the surface like Emboss.

## 12. Panel-line cutter

Draw a line on the surface and cut a groove of a set width and depth (for armour
plates, vehicle panels, scale models).

Notes: a path on the surface (as Follow draws one) swept with a small V or square
profile and cut away.

## 13. Split for painting

Cut along a colour boundary into pieces easier to paint, then glued back.

1. Split along painted regions (Split by Mask) or between colours already given.
2. Add small alignment keys on the cut so the pieces only go back one way.

Notes: Split by Mask plus the split pegs from Split for Printing.

## 14. Kit layout (sprue tree)

Lay out small parts (hands, accessories) on a frame like a model kit, joined by thin
gates, numbered.

Notes: an extension of Lay Out: a frame round the parts, gates to the nearest part
edges, embossed numbers.

## 15. Parts list

For every part: name, size, volume, and the estimated print weight and filament or
resin cost; printed or saved as a table (for planning, quoting, or a kit's
instructions).

Notes: volumes from the meshes; material density and price per kilo as preferences.
