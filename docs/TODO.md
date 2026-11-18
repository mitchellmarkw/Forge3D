# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order they came in.
Each has what's wanted, then notes on how it could fit Forge3D.

## Reference images

Pictures to design over (blueprints, photos, model sheets), shown in the view.

1. **Calibrate**: click two points whose distance you know and type it in. The
   distance can be real (a car's length: 4.5 m) or to scale (91.2 mm at 1:48), so
   everything modelled over the image comes out the right size.
2. **One picture per direction**: front, back, top, bottom, left and right, each placed
   on its own axis.
3. **Opacity**, and hide or show each picture.
4. **Active or not**: an active picture can be selected and moved like a shape. An
   inactive one ignores clicks, so you can click and change the shapes behind it (even
   when the picture is in front and faded). FreeCAD's images take the clicks, and that
   is the problem to avoid.

Notes: the picture loading from Emboss can be reused. Inactive pictures are drawn but
left out of picking. Pictures are saved in the `.f3d` file, but not exported or
printed.

## Precise part alignment

Today's Align lines shapes up by their boxes and centres. This is for joining parts:

- Pick a **face or edge on each part** (say the shoulder end of an arm's peg and the
  inside face of the torso's socket), and the first part moves so they meet: faces
  flat together, and round faces or edges sharing an axis.
- **Offsets**: a gap along and/or across. The peg is a little shorter and thinner than
  its socket, so it should sit where it would when plugged in, not jammed against the
  bottom.

Notes: build on Measure's picking (faces, edges, centres, axes).

## Quick Socket

Ball joints and peg sockets that fit after printing, without trial and error.

- **Ball socket**: give the ball's size (3 mm), and Forge3D cuts a socket that takes it
  once printed, including the clearance and how far the opening closes round the ball.
- **Peg socket**: round, or another shape such as a hexagon (so it can't turn).
- **Strain relief**, optional: for example, expansion cuts in a ball socket so it can
  open as the ball snaps in.

Notes: works like Hole and Thread (pick a face, give the sizes, it becomes a step that
can be changed later). It also needs a matching ball or peg to add, and a fit setting
as Thread has. It would pair well with the custom shape library and precise alignment.
