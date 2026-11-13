# Changelog

## 0.20.0 — Slice 11: Patterns (2026-11-13)

- **Pattern** (Design → Features, and Build → Arrange): select a shape and choose what to
  repeat:
  - **The whole shape**: copies side by side, kept together as one object;
  - **Its last step**: the hole set, pad, pocket, revolve, groove, Follow groove /
    ridge, or any shape merged or subtracted, repeated on the same part (the choice
    shown by name; it starts here when the last step is one of those).
  Then **In a row** (direction, the other way, how many, spacing), **In a grid** (two
  directions) or **Round a circle** (turning round a line across / front to back /
  straight up through a centre you set; how many; 360° spreads them evenly, less
  spreads them over that angle). Live preview.
- **Linked**: change the original (the hole's size, the pad's sketch, the groove's
  path) and every copy follows. It's a step in the history: double-click to change
  the count, spacing or circle. Resizing the shape stretches the spacing with it.
  Roll back to an earlier feature to repeat that one instead.
- File format version 16. Older files still open.
- 180 unit tests (2 new: rows, grids and circles of shapes; holes repeated and linked,
  through all, round a circle; a Follow groove repeated).

## 0.19.1 — Follow no longer hangs off the edge (2026-11-12)

- A Follow path that runs off the edge of the face it's drawn on (a circle bigger than
  the box, or a shape made smaller than its groove) froze Forge3D. Now the path is
  checked as it's laid: where the surface turns a sharp corner, or the path can go no
  further, it stops at once with "The path runs off the edge of the surface…". In the
  Follow panel that shows as the message (Apply greyed); after resizing, the shape
  shows without the groove and the step is marked ⚠ with the reason, as other
  steps that can't be made. Rounded edges are still crossed.
- Long paths are laid faster (fewer points in each fitted curve).
- 178 unit tests (1 new).

## 0.19.0 — Slice 10: Follow (grooves and ridges along a path) (2026-11-12)

- **Follow** (Design tab → Features): pick a shape, click the spot on its surface
  where the path goes, and draw the path in the sketcher (lines, arcs, circles,
  slots; joined or not; open or closed). Then choose:
  - **Groove** (cut in) or **Ridge** (raised);
  - the shape across the path: **Round**, **Square**, **V**, or **My own** (drawn in
    the sketcher: across to the sides, up out of the surface, the dashed line is the
    surface);
  - width and depth (or height).
  On a flat face the path stays exactly as drawn; on a curved surface it is laid over
  the surface from the spot you clicked, bending round with it (round a cylinder,
  over a dome). The cross-section stays upright to the surface all along. The path
  shows in orange with a live preview; clicking the shape again moves the path there.
- It's a step in the shape's history: double-click it to change the settings, the
  path or the cross-section. It stays on the same face when the shape is resized.
- File format version 15. Older files still open.
- 177 unit tests (2 new: straight, ring, ridge, V, drawn cross-section, closed
  outline with corners, resizing, saving, and a path round a cylinder).

## 0.18.0 — Slice 9: middle planes and the ribbon (2026-11-12)

- **Sketch on a middle plane**: the Sketch button's arrow offers the front, side or top
  plane through the middle of the selected shape (or of the plate, with nothing
  selected). Where the plane cuts the shape is shown dashed, to see and snap to.
  This is what Groove and Revolve need for round parts: draw a groove's outline at
  a cylinder's edge, a helper line up its middle, and Groove cuts a ring all the way
  round. (On a flat face, Groove spins round a line lying on the face, which is why it
  made two dents there.)
- Pocket can now go **both ways** (half each side of the sketch), handy from a middle
  plane.
- **Ribbon**: the toolbar is now tabs of grouped buttons, so nothing falls off the
  edge:
  - **Build**: File, Undo, Add, Handles, Combine, Arrange, Remove;
  - **Design**: Sketch (with the planes), Hole, Shell, Edges, Split, Emboss;
  - **Sculpt**: coming in a later phase;
  - **Print**: Export, Send to Slicer.
  The tab you were on is remembered. Menus and shortcuts are unchanged.
- 175 unit tests (1 new: a groove round a cylinder from its front middle plane).

## 0.17.0 — Slice 8: Revolve, Hole, Shell (2026-11-12)

- **Revolve** (in the sketch's **Make:** list): spins the outline round a helper line
  (pick the line, or have just one): knobs, bottles, wheels. Angle, the other way,
  both ways. On the plate the sketch is the side view, standing up, and the shape sits
  on the plate. On a face, **Groove** cuts it in instead.
- **Hole** (new button): pick a shape, click a flat face where the holes go (each click
  adds one), then choose:
  - **Type**: plain, counterbored (flat step for a bolt head) or countersunk (cone for
    a flat-head screw);
  - **Size**: M2 to M12 (fills in every size), made for a bolt to pass through
    (clearance) or for cutting a thread (tap drill); or Custom, typed;
  - **Depth**, or **Through all**.
  Spots can be typed (mm from the face's middle), removed, or placed exactly in a
  sketch (**Place exactly...**: points with dimensions and rules). The preview shows
  the holes as they'll be. Double-click the Drilled step later to change them.
- **Shell** (new button): hollows the selected shape out to walls of a set thickness.
  Click the faces to leave open (the top of a box makes a cup), or none for a sealed
  hollow; walls inside or outside. Open faces are kept when the shape is resized.
  Double-click the step to change it.
- Sketcher: a **Point** tool (P), for hole spots; deleting a line keeps points that
  stand on their own.
- File format version 14. Older files still open.
- 174 unit tests (5 new).

## 0.16.0 — Slice 7: Sketches, Pad and Pocket (2026-11-12)

- **Sketch** (new button): draw a flat outline and push it out (**Pad**) or cut it in
  (**Pocket**). With nothing selected the sketch goes on the plate; with a shape
  selected, click the flat face to draw on (its outline is shown dashed to snap to).
- **Drawing**: Line (L, one after another; click the first point to close), Rectangle
  (R), Circle (C), Arc (A), Slot (S). Points snap to other points, the face's corners
  and the grid. Drag points, lines and circle edges to change them. **Helper** (G)
  turns lines into dashed helper lines that rules can use but the shape ignores.
- **Rules** on what's picked: Horizontal (H), Vertical (V), Parallel, Perpendicular,
  Equal (E), Tangent (T), Coincident (O), Symmetric, Lock. Lines drawn nearly level or
  upright get Horizontal / Vertical by themselves; rectangles and slots come with
  theirs. A rule that fights the others is refused.
- **Dimensions** (D): a line's length, two points' distance, a circle's radius, two
  lines' angle; **Across** (Shift+H) and **Up** (Shift+V) measure one way only (one
  point: from the sketch's origin). Typed when added; double-click one to change it,
  and the sketch follows.
- The sketch turns **green** when every size and place is set; the panel says how many
  ways it can still move. Loose ends of an open outline show in red.
- **Finish**: Pad (length, the other way, both ways) or, on a face, Pocket (depth,
  the other way, Through all). On a face the pad is merged into the shape (or the
  pocket subtracted), as one step in its history. Undo / Redo inside the sketch are
  the sketch's own; one Undo afterwards takes the whole pad or pocket away.
- **Change a sketch later**: double-click the pad (or its Pad / Pocket step) in Objects,
  or right-click → Change.... Resizing a pad with the handles scales its sketch.
- File format version 13 (sketches and pads). Older files still open.
- 169 unit tests (8 new).

## 0.15.1 — Slice 6 fixes (2026-11-12)

- **Rounded / chamfered edges survive resizing.** Each picked edge also remembers
  where it sits within the shape's box and which way its face points, so making the
  box under a rounding much taller (or wider) keeps the same edges rounded, and
  Change the edges... shows them picked. (A box rounded on top used to lose its
  rounding once it was made 8 mm or more taller.)
- **Subtract → Through all** (tick box in Properties): straight tools (box, cylinder,
  tube, text) reach right through the shape along their own height, whatever size the
  shape becomes. Works on imported meshes too.
- **Emboss → Center on the face**: All, Top-Bottom, Left-Right (as the words read).
  New words start in the middle of the face you're looking at. Clicking still moves
  them anywhere.
- 161 unit tests (3 new).

## 0.15.0 — Slice 6: History (2026-11-12)

- **The Objects panel shows how each object was made.** Open an object to see its
  steps, oldest first (Box 1 → Rounded 1 → Subtract 1), each with a short description
  ("4 edges rounded R2", "minus Cylinder 1"). Shapes a step used as tools are listed
  under it.
- **Double-click a step to change it**: a shape's size (in Properties), the edges of a
  rounding, the words of an emboss, a split's cut.
- **Right-click a step** to switch it off or on, change a Merge / Subtract / Intersect
  into another, roll back to it, or delete it (tools go back on the plate).
- **Rollback bar**: drag it up the history to see the object as it was at an earlier
  step. Edges and Emboss added while rolled back go in at the bar. Drag the bar to the
  bottom (or right-click → Roll forward) to bring every step back. Merge, Split,
  Mirror and Group wait until the object is rolled forward.
- A step that can't be done any more (e.g. a rounding whose edges an earlier chamfer
  changed) is marked ⚠ with the reason and left out, instead of the object vanishing.
- File format version 12. Older files still open.
- 158 unit tests (4 new).

## 0.14.1 — Fillets on outside edges (coves) (2026-11-12)

- **Fillet now works on outside edges too:** it cuts a curved hollow (a cove), as if a
  cylinder of the radius, centred on the edge, were subtracted. Inside corners are
  filled as before; one Fillet can do both.
- Straight edges, round rims (a cylinder's top) and other curves all work. Picked edges
  that meet at a corner join smoothly.
- 154 unit tests.

## 0.14.0 — Slice 5e: Edges (2026-11-12)

- **Edges tool** (toolbar, Edit menu) for solid shapes, with four kinds:
  - **Rounding**: outside edges turned into a curve of a set radius (R1, R2...).
  - **Fillet**: inside corners filled with a curve of a set radius.
  - **Chamfer**: the corner cut off flat, the same distance back on both faces.
  - **Bevel**: a flat slope a set width across the face you pick, at a set angle to it.
- Pick edges in the view: they light up as the mouse passes; click to pick or drop
  one. Click a face to pick all its edges (click again to drop them). **All edges**
  picks every edge the kind can go on. Rounding only offers outside edges and Fillet
  only inside corners.
- The view shows the result as you pick and change sizes; sizes too big for the
  shape get a plain message instead.
- The result stays editable: **Change the edges...** in Properties (kind, size, other
  edges), or **Sharp edges again** to get the plain shape back. Picks follow the shape
  when it is moved, resized, or the shape under it is changed.
- Not for imported meshes (or shapes made with them): a message says so.
- File format version 11. Older files still open.
- 154 unit tests (3 new).

## 0.13.1 — Slice 5d fixes: Emboss on balls and big models; stacked letters (2026-11-12)

- **Words keep their size on a ball.** Embossed words used to squeeze together and
  break up towards the ends of a long line on a sphere. The baseline now runs round the
  surface first and each letter stands straight up from it, like a sticker smoothed on.
- **Emboss on big models is quick.** Finding where the letters go only looks at the
  triangles near them and follows the surface from triangle to triangle. On shapes over
  150,000 triangles the preview shows the words in orange on top of the shape, and the
  cut is made once, when you click Emboss / Apply.
- Words longer than the way round a shape (e.g. right round a small box) are refused
  instead of running into themselves.
- **Stack the letters (top to bottom)**: a new tick box for the Text shape and Emboss.
  Each letter stands upright under the one before, like a sign on a post.
- Saved files keep the stacked setting (older files open as before).
- 151 unit tests.

## 0.13.0 — Slice 5d: Text and Emboss (2026-11-11)

- **Text shape** (Add → Text): 3D letters lying flat on the plate, in any font installed
  on the PC. Properties has the words, font, Bold, Italic and Letter height; Width,
  Depth and Thickness stretch it like a box. Typing more words makes the block longer;
  the letters keep their size.
- **Emboss** (toolbar, Edit menu): puts raised or engraved words on the selected shape.
  The words start on the side facing you; click anywhere on the shape to move them.
  Choose the words, font, bold / italic, letter height, depth, turn, and raised or
  engraved; the view shows the result as you change things.
- Embossed words **follow the surface**: each letter stands the same depth out of (or
  into) the surface under it, and lines wrap round curves such as a cylinder or ball,
  keeping their width. Works on imported meshes too.
- Words that would hang off the edge of a shape are refused with a message in the panel.
- The result stays editable: select it and click **Change the words...** in Properties
  (or **Remove the words** to get the shape back). Resizing it evenly resizes the words.
- File format version 10 (text shapes and emboss results). Older files still open.
- Core: `Text.h` (fonts, `makeText`, `withText`, `flatLetters`), `Emboss.h`
  (`emboss::letters`, `surfaceUnder`, `embossSpec`, `withEmboss`),
  `Document::previewMesh()`, `TextSpec`, `EmbossText`, `BooleanOp::Emboss`.
- 151 unit tests (4 new).

## 0.12.0 — Slice 5c: Mesh engine (2026-11-11)

- **Big imported meshes now combine quickly.** Merge, Subtract, Intersect, Split,
  Mirror and Group on an imported shape (or anything made with one) use a mesh engine
  (Manifold) instead of the solid-modelling kernel. Subtracting a box from a 1.3 million
  triangle scan takes about 4 seconds the first time (most of that is reading the mesh
  in once) and about 1 second after; it used to freeze.
- Moving a shape inside such a result re-cuts it in about a second.
- The "this may take minutes" question from 0.11.0 is gone: it's no longer needed.
- Import repair is about 2.5x faster on big meshes (1.3M triangles: under 2 s).
- Exporting big meshes is about 5x faster.
- Primitives combined with a mesh are turned into fine triangles (within 0.01 mm), so
  results are watertight and export cleanly.
- Core: `MeshKernel.h` (`MeshSolid`, `meshkernel::` operations),
  `Document::usesMeshEngine()`, `meshSolid()`, `localBounds()`, `previewResult()`.
  New dependency: Manifold 3 (Apache-2.0).
- 147 unit tests (4 new).

## 0.11.0 — Slice 5b: Import with repair (2026-11-11)

- **Import** (File menu, toolbar, Ctrl+I): adds shapes from **STL** (binary or text),
  **3MF** (every object, with its name; components, transforms and units handled) and
  **OBJ** (one shape per object / group; polygons split into triangles). Pick several
  files at once, or **drag files onto the window** (dropping a .f3d opens it).
- **Automatic repair** on the way in: duplicate points joined, empty and repeated
  triangles removed, triangles turned to match their neighbours, inside-out pieces
  turned the right way, holes filled. A message lists what was repaired.
- Imported shapes land on the plate, to the right of what's there (several shapes from
  one file keep their places relative to each other), named after the file or object.
- They work like any shape: move, turn, **resize** (Width / Depth / Height, handles,
  Uniform), Mirror, Duplicate, Group, Align, Export, and **Merge / Subtract / Intersect /
  Split** with other shapes. One that can't be made watertight can be moved, resized
  and exported, but not combined or split (Properties says so).
- Saved inside the .f3d file (file format version 9). Older files still open.
- **Units**: STL and OBJ files don't record their units. If one comes in under 2 mm
  across (usually saved in metres, e.g. from Blender, or in inches), Forge3D asks
  which units it was made in and scales it (metres x1000, inches x25.4, cm x10, or keep).
- Typing a size now always changes the field you typed in. (Before, a size the box
  couldn't show exactly, such as 0.003 mm, could be changed instead, and your number
  seemed to reset.)
- Combining, splitting, mirroring or grouping more than 20,000 triangles of imported
  mesh asks first, since it can take minutes, and logs when it starts.
- Sample files for testing are in `test-sheets/samples`.
- Core: `MeshSpec` / `MeshData`, Import.h (`readStl`, `readObj`, `read3mf`,
  `repairMesh`, `importFile`), `makeMeshSolid()`.
- 142 unit tests (4 new).

## 0.10.0 — Slice 5a: Export and Send to Slicer (2026-11-11)

- **Export** (File menu, toolbar, Ctrl+E): saves the selected shapes, or everything if
  nothing is selected, as **3MF** (best for slicers: keeps names and millimetres),
  **STL** (binary) or **OBJ**. Fine triangles (within 0.01 mm of the true surface);
  points shared between triangles so the files are watertight.
- **Send to Slicer** (File menu, toolbar): writes a 3MF to a temporary folder and opens
  it in your slicer. The first time, pick from the slicers found on the PC (PrusaSlicer,
  Bambu Studio, OrcaSlicer, Cura, Creality Print, SuperSlicer), the program Windows uses
  for .3mf files, or browse to any program. **File > Choose Slicer...** changes it.
- If a shape's surface isn't closed, Export says which one.
- The folder and file type used last are remembered.
- Core: Export.h (`exportMeshes`, `writeMeshes`, `formatForPath`).
- 138 unit tests (4 new); the 3MF files were also checked with an independent reader
  (trimesh): watertight, right way out, names kept.

## 0.9.0 — Slice 4c: Duplicate, Mirror, Group, move several (2026-11-11)

- **Duplicate** (Ctrl+D, toolbar): copies of the selected shapes, placed to the right of
  everything on the plate. Combined shapes are copied with all their parts.
- **Copy / Cut / Paste** (Ctrl+C / Ctrl+X / Ctrl+V, Edit menu). Paste after Copy lands
  clear of everything; the first Paste after Cut puts the shapes back where they were.
- **Mirror** (toolbar dropdown, Edit menu): flip the selected shapes in place, left /
  right, front / back or upside down. "Remove the mirror" (Properties) flips it back.
- **Group** (Ctrl+G): shapes move, turn and resize as one without merging. **Ungroup**
  (Properties, Separate, or Ctrl+Shift+G).
- **Several shapes at once**: with two or more selected, the Move and Rotate handles
  appear at their shared middle and move / turn them together (one Undo); dragging one
  of them on the plate slides them all. To resize several together, Group them.
- Fixed: long hints (e.g. "2 shapes selected...") no longer widen the Properties panel.
- File format version 8 (Mirror and Group). Older files still open.
- Core: `BooleanOp::Mirror` / `Group`, Arrange.h (`captureTree`, `cloneTree`,
  `addCopies`, `clearOfPlate`, `mirrorBodies`, `groupBodies`).
- 134 unit tests (5 new).

## 0.8.0 — Slice 4b: handles everywhere (2026-11-11)

- **Parts inside combined shapes get handles.** Select a part in Objects (e.g. the
  cylinder inside "Merge 1"): Move, Rotate and Resize (W / E / R) work on it in place,
  along the plate's directions, and the combined shape rebuilds as you drag. Esc during
  a drag puts it back; one Undo per drag.
- **Combined shapes get Resize handles.** Drag a face handle to stretch the whole shape;
  the opposite side stays still. Everything inside stretches with it (a round peg on a
  block stretched along X becomes oval). With **Uniform** on, it scales evenly.
- Stretching one way only isn't possible when a part inside is turned at an odd angle
  (or for a torus across its ring): the shape then scales evenly and the status bar
  says why.
- Split halves stretch with their cut.
- Core: `scaledTree()` / `scaledSpec()` (Scale.h), `EditBodiesCommand`,
  `movedOnPlate()` and `rotatedAboutPlateAxis()` for parts inside a shape.
- 129 unit tests (6 new).

## 0.7.1 — Drag the Split plane (2026-11-11)

- In the Split tool, **drag the yellow plane** (or its arrow) to slide it along its
  direction. Snaps to 1 mm; hold Alt to move freely; Esc during the drag puts it back.
  The slider and the "From the middle" field follow.
- While the Split tool is open, clicking in the view no longer changes the selection
  (and so no longer closes the tool by accident). Cancel or Esc leaves.
- After a split, the arrow-side half is selected on its own, so the Move / Rotate /
  Resize handles (and W / E / R) work on it straight away, as after Merge.

## 0.7.0 — Slice 4a: Split (2026-11-11)

- **Split** (toolbar and Edit menu): select one shape, click Split, and a see-through
  yellow plane with an arrow appears through its middle. In the panel on the right:
  - **Direction**: Flat, Across X, Across Y, or any tilt around X and Y.
  - **Position**: a slider (end to end of the shape) or a typed distance.
  - **Keep**: both halves as two shapes, or only one side.
  - **Split** cuts; **Cancel** or Esc leaves without cutting. One Undo puts the shape back.
- Halves are named after their side ("Box 1 (top)", "Box 1 (bottom)", left/right,
  front/back) and stay editable: open one in Objects to change the original shape.
- **Move the cut...** (Properties of a half) reopens the plane to change where that half
  was cut. **Remove the cut** gives back the whole shape.
- Splitting a combined shape works too; keeping both halves copies all its parts.
- File format version 7. Older files still open.
- Fixed: the "empty plate" hint still mentioned Add Box / Add Cylinder.
- Core: `BooleanOp::Split` with a plane, `splitBody()`, `duplicateTree()`,
  `Document::setBoolean()` (EditBodyCommand now updates a result's settings too).
- 123 unit tests (8 new).

## 0.6.1 — Oval shapes and the pyramid (2026-11-11)

- **Pyramid** in Add Shape: base width and depth, top width and depth, height. Top 0 x 0
  is a point; one of them 0 is a roof-like ridge; both above 0 is a flat top.
- **Oval radii** for every round shape, like the cylinder already had:
  - Sphere: Radius X, Y and Z (eggs and squashed balls).
  - Cone: bottom radius X / Y and top radius X / Y.
  - Torus: ring radius X / Y (an oval ring), tube radius across and up (Z).
  - Tube: outer radius X / Y and inner radius X / Y.
- The **Round** checkbox now appears for all of these. Ticked, the X / Y radii (a
  sphere: all three) change together, in Properties and with the Resize handles; ticking
  it on an oval shape makes it round again. Unticked, each radius changes on its own.
- File format version 6. Files from 0.6.0 (format 5) still open.
- 115 unit tests (4 new; the shape tests now cover every oval and pyramid variant).

## 0.6.0 — Slice 3d: shape library (2026-11-11)

- **Add Shape** dropdown in the toolbar (and a new **Add** menu) replaces Add Box / Add
  Cylinder: Box, Cylinder, **Sphere**, **Cone**, **Torus**, **Tube**, **Wedge**.
- Every new shape has its own Size fields in Properties, works with the Resize handles
  (a sphere stays a sphere; a cone keeps its taper; a tube keeps its wall thickness; a
  torus side handle widens the whole ring, the top handle thickens it), Uniform scale,
  Merge / Subtract / Intersect, Align Shapes, Save and Open.
- A cone's top (or bottom) radius can be 0 for a point, or more for a flat cut-off top.
- Sizes that can't work are refused with a message (e.g. a tube's hole bigger than the
  tube, a torus with no hole).
- File format version 5 (adds the new shape types). Older files still open.
- Core: one table per shape (`specFields()`, `specValue()`, `setSpecValue()`, `scaled()`,
  `defaultSpec()`), so the panel, files and scaling handle every shape the same way;
  `halfExtents()`.
- 111 unit tests (7 new).

## 0.5.1 — Turning centre and Align Shapes (2026-11-11)

- **Combined shapes turn about what you see.** Gizmo rings and typed angles turn a
  Merge, Subtract or Intersect result about the centre of its actual solid; cut-away
  shapes don't count. A cube with a taller cylinder subtracted turns about the cube's
  middle. The gizmo sits there too.
- **Align Shapes** (toolbar dropdown and Edit menu). Select shape 1, Ctrl+click shape 2
  (or more), then pick: **Meet At Face**; **Align Face Center XYZ / X / Y / Z** (halfway
  in); **Center XYZ / X / Y / Z**. Shape 1 stays put. One Undo step per align.
- Shape bounds are now exact for tilted shapes, so **Sit on plate** puts a tilted cylinder
  exactly on the plate.
- Core: `Document::visibleCentre()`, `shapeBounds()`, `withRotation()`, a
  `rotatedAboutPlateAxis()` overload taking a centre, and `Align.h`.
- 104 unit tests (8 new).

## 0.5.0 — Slice 3c: combine shapes (2026-11-11)

- **Multi-select**: Ctrl+click in the view or the Objects list adds or removes shapes.
  The order you pick them in is kept.
- **Merge**, **Subtract** (keeps the first shape picked, cuts the others out of it) and
  **Intersect** in the toolbar and Edit menu. Exact solids from OpenCASCADE, with faces on
  the same surface unified so merges have no seams. Combining shapes that would leave
  nothing (e.g. Intersect of shapes that don't touch) is refused with a message.
- **Results stay editable**: the Objects panel is now a tree; a result holds its parts.
  Select a part to see it see-through in place and edit its size and position in
  Properties; the result rebuilds. Results can be combined again (nesting).
- **Separate** (toolbar or Properties) splits a result back into its parts, each left
  where it appeared, even after the result was moved or turned.
- Move and Rotate work on results with the gizmo; Resize handles are not shown for results.
- Delete removes a result with all its parts; parts can't be deleted on their own.
- File format version 4 stores results (operation, parts, pivot).
- Core: `BooleanSpec` on `Body`; the document tracks which result each part belongs to,
  caches exact shapes, and rebuilds results when a part changes. `CombineCommand`,
  `SeparateCommand`, `placementFromTransform()`.
- 96 unit tests (13 new).

## 0.4.2 — Fix (2026-11-11)

- Round checkbox now unticks when Undo, Redo or a typed radius turns the selected cylinder
  oval (found in test sheet 3b2, check 15).

## 0.4.1 — Uniform scale and elliptical cylinders (2026-11-11)

- **Uniform scale**: toolbar / Edit menu toggle (key **U**) and a matching checkbox in
  Properties → Size. Resize handles and typed sizes then scale every dimension by the same
  factor. The opposite side stays put and the base stays on the plate.
- **Elliptical cylinders**: cylinders have **Radius X** and **Radius Y**. With **Round**
  ticked (the default for round cylinders), both change together; untick it to change
  each on its own. Ticking Round again makes the cylinder round using Radius X. Built as an
  exact elliptic extrusion in OpenCASCADE (a true circular cylinder when the radii match).
- Resize handles on a box's sides now keep the opposite side fixed at base level, so
  uniform scaling from a side handle does not push the body into the plate.
- File format version 3: cylinders store radiusX and radiusY. Older files open with round
  cylinders.
- Properties: no blank gap under a box's sizes after showing a cylinder.
- 83 unit tests (6 new).

## 0.4.0 — Slice 3b (2026-11-11)

- Left-drag any body to slide it across the plate (selects it too).
- Gizmo around the selected body, with three tools in the toolbar and Edit menu:
  **Move (W)** arrows along X / Y / Z, **Rotate (E)** rings around X / Y / Z (about the
  plate's axes, through the body's centre), **Resize (R)** a handle on each face; the
  opposite face stays put (cylinders: radius grows around the centre line).
- Hover highlight on handles; the dragged handle turns yellow and the others fade.
- Snapping (toolbar **Snap**, on by default): 1 mm moves, 15° turns, whole-mm sizes.
  Hold **Alt** while dragging to move freely.
- Every drag is one undo step (`EditBodyCommand`); **Esc** during a drag cancels it.
- Live status while dragging ("Move X 12.0 mm", "Rotate around Z 45°", "Resize Height 37 mm").
- Core: `eulerXYZ()`, `rotatedAboutPlateAxis()`, `resizedFace()`, `dimension()`.
- View: `Gizmo` (screen-constant geometry, hit testing) and drag maths
  (`closestOnLine`, `intersectPlane`, `screenAngle`, `snapTo`).
- 77 unit tests (18 new).

## 0.3.1 — Slice 3a revision (2026-11-11)

- Rotation about all three axes: Properties has a Rotation group (Around X, Y, Z). Turns
  are applied X, then Y, then Z, about the plate's fixed axes, around the body's centre.
- **Sit on plate** button: moves the selected body so its lowest point touches the plate.
- One transform (`core/Transform.h`, `placementTransform`) now drives drawing, picking and
  bounds, so what you see is exactly what you click.
- File format version 2: placements store rotationX, rotationY and rotationZ. Version 1
  files still open (no tilt).
- A file passed on the command line opens after the main window appears, so error
  messages have a window behind them.
- 59 unit tests (10 new: transforms, tilted bounds and picking, sit on plate, format v1/v2).

## 0.3.0 — Slice 3a (2026-11-11)

- Selection: left-click a body to select it (cyan glow); click empty space or press Esc to
  clear. New bodies are selected as they are added. Selection is not part of undo.
- Objects panel (left): every body by name; click to select.
- Properties panel (right): name, size (box width/depth/height, cylinder radius/height) and
  position (X, Y, lift, rotation). Each typed change is its own undo step.
- Delete key / Delete button removes the selected body.
- `.f3d` files: a zip holding `document.json` (format version 1, units mm). File → New, Open,
  Save, Save As; Save / Discard / Cancel before losing changes; file name in the title;
  last folder remembered. Saving writes a temporary file first, so a failed save never
  damages the previous file. Damaged, foreign or newer-format files give a clear message.
- Opening a `.f3d` passed on the command line (drag a file onto Forge3D.exe).
- Ray picking in core (`Picking.h`); `Document::replaceAll()`; `SetPlacementCommand` can be
  non-merging for typed values.
- Log records Undo, Redo, Saved and Opened.
- New dependencies: miniz (zip) and nlohmann-json.
- 49 unit tests (15 new: picking, file format, camera rays, typed moves).

## 0.2.0 — Slice 2 (2026-11-10)

- `Document`: an ordered list of bodies (box or cylinder parameters, placement, name),
  with change notifications, unique naming, input validation, and a geometry cache that
  re-tessellates only when a body's parameters change.
- Commands: Add, Delete, Move, Resize, Rename, and Macro (several changes as one step).
- `CommandStack`: one undo history for everything, with merging of repeated moves
  (for drags), macros, a 200-step limit, and saved-state tracking.
- Viewport draws any number of bodies, each in its own colour, and re-uploads only
  meshes that changed.
- Edit menu and toolbar: Undo / Redo with labels ("Undo Add Box"), Ctrl+Z, Ctrl+Y and
  Ctrl+Shift+Z. `*` in the title for unsaved changes; File → New and closing ask first.
- Temporary test tools: Add Box, Add Cylinder, Make Last Taller, Delete Last, Clear Plate.
- Startup timing (`Startup:` lines) and one `Command:` line per edit in the log.
- `--demo-scene` start-up option for screenshots.
- 34 unit tests (21 new for the document and undo stack).

## 0.1.0 — Slice 1 (2026-11-10)

- CMake + vcpkg project with presets for Windows (Debug, Release) and Linux (dev checks).
- `forge_core`: box and cylinder primitives (OpenCASCADE) and B-rep → triangle mesh tessellation.
- `forge_view`: Z-up orbit camera (orbit, pan, zoom, fit).
- Forge3D app: Qt 6 window with an OpenGL 3.3 viewport, 220 mm build-plate grid with X/Y axes,
  and a 40 × 30 × 20 mm starter box.
- Navigation: right-drag orbit, middle-drag or Shift+right-drag pan, wheel zoom, F fit, Home reset.
- Log file and Windows crash dumps under `%LOCALAPPDATA%\Forge3D`.
- `--screenshot <file>` option for automated visual checks.
- 13 unit tests (primitives, tessellation, camera).
- GitHub Actions Windows build that runs the tests and uploads a ready-to-run zip.
