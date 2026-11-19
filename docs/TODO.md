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
