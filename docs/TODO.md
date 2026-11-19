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

## Split by mask

Cut a part in two along a line painted on it, rather than along a flat plane.

- **Paint a mask** on the part (as the Sculpt tool's Mask brush does), then **Split by
  Mask**: the masked area becomes one piece and the rest another.
- **The holes are closed**: each piece is capped where it was cut, so both are solid
  and printable.

Notes: the Sculpt mask (each point's shade) already exists and is saved with sculpt
steps; the split could be a step like Split, keeping the painted mask so it can be
changed. The cut follows the mask's edge (where the shade crosses half), smoothed so
it isn't jagged along the triangles. Capping a flat-ish cut is a fill of the edge
loop; a strongly curved one needs a surface spanning it. Pegs or keys (as Split to Fit
adds) may be wanted on the cut.

