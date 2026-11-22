# Forge3D architecture

This file is the hand-off note between work sessions. Keep it current when the
structure changes.

## Layers

```
src/app    Qt Widgets application: MainWindow, Viewport (OpenGL 3.3 core), logging, crash dumps
   │  uses
src/view   Viewport maths that need Qt GUI types but no window (OrbitCamera)
   │
src/core   Geometry and (soon) the document model. OpenCASCADE only, no Qt.
```

Rules:

- `core` must never include Qt headers. It is the part that will grow largest and must
  stay testable and reusable (command-line tools, Python bindings later).
- The UI never edits geometry directly. Starting in slice 2, every change goes through a
  command on the document so undo and recompute work everywhere.
- Code stays portable C++20 even though only Windows is shipped: the core and tests are
  compiled and run on Linux before each hand-off. Windows-only code lives in
  `src/app` behind `#ifdef _WIN32` (today: `CrashHandler.cpp`).

## Key decisions

| Decision | Choice | Why |
| --- | --- | --- |
| B-rep kernel | OpenCASCADE | Only mature free kernel; FreeCAD's engine |
| UI | Qt 6 Widgets | One UI technology, all C++ |
| Rendering | OpenGL 3.3 core via `QOpenGLWidget` | Simple, runs on nearly every Windows GPU from the last 12 years |
| Dependencies | vcpkg manifest (`vcpkg.json`), pinned baseline | One command fetches everything; same versions locally and in CI |
| Tests | Catch2 v3, `ctest` | Header-light, readable test names |
| Units | Millimetres, Z up, build plate at Z = 0 | Matches 3D printing |
| License | GPL-3.0-or-later | Allows reuse of GPL code (SolveSpace, Blender) |

## Document and undo (slice 2)

```
UI action ──> Command ──> CommandStack::execute() ──> Command::redo(Document)
                                │                            │
                       undo / redo history          Document notifies listeners
                                                             │
                            MainWindow::refreshScene() <─────┘ (queued, once per burst)
                                     │
             Document::mesh(id) (cached) + placement ──> Viewport::setItems()
```

- **`Document`** (`core/Document.h`) owns the bodies. Each `Body` holds an id, a name,
  a `PrimitiveSpec` (box or cylinder parameters) and a `Placement`. Geometry is never
  stored as the source of truth: it is rebuilt from the parameters.
- **Mutators validate first, then change.** A call that throws leaves the document as
  it was. Commands rely on this.
- **Geometry cache:** `Document::mesh(id)` tessellates on first use and caches the result
  until that body's spec changes. Moving a body does not re-tessellate; the viewer applies
  the placement as a model matrix. A new `shared_ptr` means new geometry, which is how the
  viewport knows to re-upload.
- **Commands** (`core/Commands.h`) capture their "before" state on the first `redo()`.
  `SetPlacementCommand` merges with the next move of the same body, so a whole drag is
  one undo step. `MacroCommand` groups steps (e.g. Clear Plate).
- **`CommandStack`** is the only undo history, shared by all modes. It tracks the saved
  ("clean") index for the `*` in the title, and refuses to merge into the saved step so
  that state stays reachable.
- **The UI never calls Document mutators directly**, except File → New and File → Open,
  which replace everything and clear the history.

## Selection, panels and files (slice 3a)

- **Selection lives in `MainWindow`**, not in the document, and is not undoable. A left click
  in the viewport emits the ray under the cursor; `forge::pick()` (core) finds the nearest
  body by testing triangles in each body's own coordinates.
- **`PropertiesPanel` never edits the document.** It emits requests; `MainWindow` turns each
  into a command (typed moves use `SetPlacementCommand(..., mergeable=false)` so each edit is
  one undo step) and then refreshes the panel from the document.
- **`.f3d` format** (`core/FileFormat.h`): a zip (miniz) holding `document.json`
  (nlohmann-json) with `format`, `formatVersion` (now 1), `createdBy`, `units` and `bodies`.
  The zip is built in memory and written with C++ streams so Windows paths with any
  characters work; it is written to `<name>.saving` and then renamed over the target.
  Readers reject newer `formatVersion`s and unknown shape types with a clear message.
  When the format changes, bump `kFormatVersion` and keep reading older versions.
- **Placement → transform:** `placementTransform()` (`core/Transform.h`) is the single
  definition of where a body is: translate to (x, y, z), then rotate about the body's centre
  (`pivotOf(spec)`), X first, then Y, then Z, about fixed axes (R = Rz·Ry·Rx). The viewport's
  model matrix, `Document::worldBounds()` and `pick()` all use it.
- **Format history:** v1 (0.3.0) stored only rotationZ; v2 (0.3.1) adds rotationX and
  rotationY; v3 (0.4.1) stores cylinder radiusX / radiusY instead of radius. `fromJson`
  reads all three.
- Opening replaces all bodies with `Document::replaceAll()` (checks everything first) and
  clears the undo history.

Linux development builds need miniz from outside the system packages: install it to a
prefix and configure with `FORGE3D_EXTRA_PREFIX=<prefix> cmake --preset linux-dev`.

## Gizmo and dragging (slice 3b)

```
Viewport (mouse)  --leftPressed(ray, handle)-->  MainWindow drag controller
                  --leftDragged(ray, pixel, mods)-->   computes the new Body from the
                  --leftReleased(wasDrag)-->           state at drag start, previews it
                                                       on the Document, then records ONE
                                                       EditBodyCommand(before, after)
```

- **`view::Gizmo`** is rebuilt from the camera each frame, so it is a constant size on
  screen. It produces camera-facing triangles (no wide-line support needed in OpenGL core)
  and does hit testing in pixels. Arrows and rings use the **plate** axes; face handles use
  the **body's** axes (they follow a tilted body).
- **Drags always start from the body as it was at mouse-down** (`Drag::before`), never
  from the previous preview, so snapping is exact and nothing accumulates rounding.
- **Preview edits the Document directly** (not undoable); on release one
  `EditBodyCommand(before, after)` goes on the stack, and Esc restores `before`.
  `EditBodyCommand` carries both states, so recording it after the preview is a no-op redo.
- **Rotation drags** sum on-screen angle changes around the projected centre (so turns past
  180° keep going) and flip the sign when the axis points away from the viewer, so the
  grabbed part of the ring follows the mouse. The result goes through
  `rotatedAboutPlateAxis()`, which composes the turn with the current rotation and
  converts back to X/Y/Z angles (`eulerXYZ()`; at exactly ±90° about Y, Z is reported as 0).
- **Resizing** keeps an anchor fixed in the plate (`resizedFace()` solves for the new
  placement), so it works on tilted bodies too. Anchors: the opposite end for top/bottom
  handles; the opposite side at base level for box side handles; the centre line at the base
  for cylinder side handles. `ResizeOptions` adds `uniform` (scale everything by one factor,
  never below `minValue`) and `keepRound` (cylinder side handles change both radii).
- **Cylinders** have `radiusX` / `radiusY`. `CylinderSpec{r, h}` is round and
  `CylinderSpec{rx, ry, h}` elliptical (constructors, so old two-value call sites keep their
  meaning). Unequal radii are built as an elliptic face extruded with `BRepPrimAPI_MakePrism`.
- **"Round" is UI state**, not stored: when a cylinder is selected it starts ticked if the
  radii are equal. "Uniform" is a window-wide setting shared by the toolbar and Properties.

## Combined shapes (slice 3c)

- A **result** is a `Body` with `boolean` set (`BooleanSpec`: op, operand ids, pivot).
  Its parts stay in `Document::bodies()`; **which body is inside which is derived** from
  the results' operand lists (`parentOf()`), so it can never disagree with them. A part
  belongs to at most one result; results can be parts of other results.
- **Coordinates:** a part's placement is relative to its result. `localTransform(id)` maps
  a body into its parent; `worldTransform(id)` chains up to the plate. Drawing, picking and
  bounds use `worldTransform`. Only top-level bodies are drawn and picked; a selected part
  is drawn as a see-through "ghost" on top.
- **Geometry:** `Document::shape(id)` is the exact `TopoDS_Shape` in body coordinates,
  cached. A result's shape = its parts' shapes, each moved by its `localTransform`, then
  Fuse / Cut / Common (BRepAlgoAPI), then `ShapeUpgrade_UnifySameDomain`. Changing a part's
  spec or placement clears the caches of every result above it (`invalidate()`).
- **Pivot (stored):** the placement of a result is defined about the pivot stored when it
  was made, so editing a part never makes a tilted result jump.
- **Turning centre (slice 3c2):** rotation (gizmo rings, typed angles) turns a result
  about `Document::visibleCentre(id)`, the centre of the exact bounding box of its solid,
  so cut-away parts don't count. `rotatedAboutPlateAxis(body, axis, deg, centre)` and
  `withRotation(body, angles, centre)` build the new transform with that point fixed and
  convert back with `placementFromTransform(t, pivotOf(body))`, so the stored pivot never
  has to change. New results store their visible centre as the pivot. The gizmo is drawn
  at the visible centre.
- **Bounds:** `worldBounds(id)` is exact (`BRepBndLib::AddOptimal` on the moved solid,
  not the mesh), so a tilted cylinder's box hugs it. `shapeBounds()` does the same for any
  shape.
- **Align Shapes** (`Align.h`): `alignedPlacement(doc, fixed, moving, mode)` moves only the
  position of `moving`, using both world bounding boxes. The facing axis is the one with
  the largest centre gap relative to the combined half sizes (ties: Z, then Y). Meet At
  Face slides along it until the boxes touch; Face Center modes put the moving centre on
  the fixed box's face on the side it is already on (halfway in); Center modes line up
  centres. The app applies all moves in one macro (one Undo).
- **Commands:** `CombineCommand` inserts the result (its parts leave the plate);
  undo removes it (they come back). `SeparateCommand` removes a top-level result and gives
  each part the placement that keeps it where it was (`placementFromTransform()`).
  Delete removes a result first, then its parts (`withDescendants()`), in one macro.
- **Selection** is an ordered list in `MainWindow` (the first picked is what Subtract keeps).

## Shape library (slice 3d)

- `PrimitiveSpec` is a variant of seven specs (Box, Cylinder, Sphere, Cone, Torus, Tube,
  Wedge). All sit centred on X/Y with their lowest point at Z = 0.
- `Body.cpp` holds one `Traits<T>` table per spec: display name, file type name and the
  length fields (member pointer, file key, label, tooltip, whether 0 is allowed). The
  Properties panel (three generic rows), the file format, validation and uniform scaling
  all go through `specFields()` / `specValue()` / `setSpecValue()` / `scaled()`, so a new
  primitive needs: its struct, a `Traits` entry, `makeX()` in Primitives, and cases in
  `halfExtents()` and `resizeOne()` (both `static_assert` on unhandled types).
- **Round groups (0.6.1):** a field's `roundGroup` links the X / Y (/ Z) radii of one
  round part (e.g. a cone's bottom radii are group 1, its top radii group 2). "Round"
  in the panel and `ResizeOptions::keepRound` change a group together; `isRound()` and
  `madeRound()` work on groups. Ovals: ellipses all start at +X so lofts don't twist;
  oval cone = ruled `BRepOffsetAPI_ThruSections`; ellipsoid = sphere stretched with
  `BRepBuilderAPI_GTransform`; torus with an oval tube is revolved, with an oval ring it
  is swept (`BRepOffsetAPI_MakePipe`). The pyramid is sewn from planar faces (repeated
  top corners dropped, so points and ridges work). Torus validity: tube radius < (small
  ring radius)^2 / (large ring radius), so the hole never closes.
- `halfExtents(spec)` gives the bounding box; `pivotOf()` and the gizmo handles use it.
  `dimension()` is the full height for Z, and for X/Y the full width (box, wedge) or the
  radius (round shapes).
- OpenCASCADE: sphere / cone / torus from `BRepPrimAPI`; equal cone radii build a
  cylinder (OCCT refuses that cone); the tube is a ring face extruded; the wedge is a
  right-triangle face extruded along Y.

## Split (slice 4a)

- A **Split result** is a combine result with `op = Split`, one operand and a plane
  (`planePoint`, `planeNormal`, in the result's coordinates; the kept side is the one
  the normal points to). Geometry: the part, placed, `BRepAlgoAPI_Common` with a huge
  box on the kept side, then UnifySameDomain.
- `splitBody()` (Split.h) previews both halves first and refuses, changing nothing, if
  either is empty; then, in one macro, it either wraps the shape in one result or, for
  both halves, adds a `duplicateTree()` copy and wraps each in a result.
- Moving a cut later is `withSplitPlane()` + `EditBodyCommand`; the plane is given on
  the plate and converted into the result's coordinates. `Document::setBoolean()`
  changes a result's pivot / plane but never its parts.
- The UI keeps the plane as tilts around X and Y plus an offset from the shape's
  visible centre (`planeFromTilts()` / `tiltsOfPlane()`). The plane and arrow are extra
  display items; `DisplayItem::alpha < 1` draws see-through items after the solids,
  depth-tested but not depth-writing.

## Handles on parts and combined shapes (slice 4b)

- A part's handles work in plate directions; the drag converts to the part's own
  placement inside its parent: `movedOnPlate(body, delta, parentWorld)` and
  `rotatedAboutPlateAxis(..., parentWorld)`. Resize uses `resizedFace()` unchanged: it
  keeps the anchor fixed in the parent's coordinates, so also on the plate.
- Resizing a combined shape is `scaledTree(doc, id, factors, anchor)`: the shape's own
  placement stays; each part gets new sizes and position, `p' = S (p + c - A) + A - c'`
  (c, c' the part's pivot before / after). A nested shape keeps its placement and
  scales its contents about the anchor seen from inside it; a Split plane's point
  scales and its normal scales inversely. Uneven factors need each part's axes to lie
  within directions that stretch alike (any turn about Z is fine when only Z
  stretches); otherwise `NotScalable`, and the app falls back to even scaling.
- The drag computes from a snapshot `Document` taken at the start (the live one holds
  the preview), previews every changed body, and records one `EditBodiesCommand`.

## Arrange: Duplicate, clipboard, Mirror, Group (slice 4c)

- The clipboard holds `BodyTree`s (a shape and its parts, parts first) captured by value,
  so pasting works after the originals are deleted. `cloneTree()` gives new ids and
  names ("Box 1 copy") and remaps operands; `addCopies()` inserts trees as one step.
- **Mirror** is a result with one operand and a plane through the shape's visible
  centre: geometry = the placed part transformed by `gp_Trsf::SetMirror`. Scaling keeps
  it exact when the stretch is along or across the mirror (always true for the plate-
  axis planes the UI makes).
- **Group** is a result whose shape is a `TopoDS_Compound` of the placed parts (no
  boolean), so they move, turn, scale, combine and save as one.
- Several selected shapes share a gizmo at their combined bounds' centre (plate axes,
  no resize); the drag previews every shape and records one `EditBodiesCommand`.

## Export (slice 5a)

- `exportMeshes()` tessellates a copy of each shape (`BRepBuilderAPI_Copy` without its
  mesh, so display triangles stay as they are) at export quality (0.01 mm, 0.15 rad),
  moves the points onto the plate and welds them on a 1e-5 mm grid; triangles that
  collapse are dropped and edges not used exactly twice are counted (`openEdges`).
- Writers: binary STL (one solid), OBJ (one `o` per shape, 1-based global indices) and
  3MF (zip of `[Content_Types].xml`, `_rels/.rels`, `3D/3dmodel.model`, one object and
  build item per shape, `unit="millimeter"`). All write to `<file>.saving` and rename.
- Send to Slicer writes a 3MF under the temp folder and starts the chosen program
  (`QProcess::startDetached`) or the .3mf file association (`QDesktopServices`).
  The choice is kept in QSettings (`slicer/path`; empty = file association).

## Import (slice 5b)

- An imported shape is a primitive like any other: `MeshSpec` holds a shared, never-
  changed `MeshData` (welded triangles, centred like a primitive, natural size) plus
  width / depth / height, so sizing, handles, Uniform and scaling all work unchanged.
- Drawing, bounds, picking and export use the triangles directly (`displayMesh()`); a
  B-rep (`makeMeshSolid()`: one plane face per triangle, vertices and edges shared, no
  sewing) is only built when a boolean needs it. Open meshes give a shell and are kept
  out of booleans and Split.
- `repairMesh()`: weld on a grid of 1e-6 of the size, drop empty / repeated triangles,
  orient each piece consistently through two-triangle edges, fill each simple boundary
  loop with a fan around its centroid, turn pieces with negative volume, count what's
  left open or non-manifold, then centre.
- Readers: STL (binary if the size matches the triangle count, else text), OBJ (points
  shared across objects are copied per object), 3MF (a small tag scanner; components
  and build-item transforms composed; `unit` converted to mm).
- .f3d (version 9): each mesh body's triangles are stored as `meshes/<id>.bin`
  (`F3DMESH1`, counts, size, closed flag, floats, indices); the JSON names the entry.

## Mesh engine (slice 5c)

- Two geometry kernels. OCCT B-reps stay the kernel for primitives and their results.
  Anything with an imported mesh in it uses **Manifold** (`MeshKernel.h`, wrapping
  `manifold::Manifold` as a shared, immutable `MeshSolid`), because a B-rep boolean on a
  million plane faces takes minutes and Manifold takes about a second.
- `Document::usesMeshEngine(id)`: true for a `MeshSpec`, and for a result with any mesh-
  engine part. `meshSolid(id)` builds (and caches, invalidated like the shape cache) the
  solid: a `MeshSpec` from its welded triangles scaled to its size; a primitive taking
  part from its export-quality triangles in its own coordinates; a result by combining
  its parts' solids moved by their placements (`combineMeshes`: Merge = batch union,
  Subtract, Intersect, Split = trim by plane, Mirror, Group = triangles side by side).
- `previewResult(spec)` answers "empty?" and bounds before a result is made, for either
  kernel. The last mesh-engine preview is kept (with copies of its parts) so making the
  result straight after reuses it instead of working it out again.
- Display, bounds, centre and export of mesh-engine results read the solid's triangles
  (`meshkernel::displayMesh` / `triangles`); `shape(id)` still builds a B-rep from them
  if something asks, but nothing on the normal paths does.
- Repair (`repairMesh`) welds by sorting grid cells and matches edges with a sorted edge
  table, not maps: 1.3M triangles in under 2 s. Export skips welding for meshes that
  already share corners and counts open edges by sorting.
- Manifold refuses meshes that aren't closed (`MeshEngineError`); open imports are
  already kept out of combining and Split.

## Text and Emboss (slice 5d)

- Letters come from the fonts installed on the PC through OCCT (`Font_FontMgr`,
  `StdPrs_BRepFont`, `StdPrs_BRepTextBuilder`, FreeType). `Text.h` hides it; one mutex
  guards the font code (not thread-safe). The first font scan takes a second or two, so
  the app starts it on a background thread after the first frame (`text::warmUp`).
- `TextSpec` is a primitive like a box: the laid-out words (font size 10, cached) are
  stretched to width x depth and pushed up `height`. Changing the words or font goes
  through `text::withText`, which keeps the letter size and stretch, so the block grows
  with the words. Stacked (vertical) text puts a line break after every letter
  (`text::stacked`, UTF-8 aware) and centres the lines. Saved as type "text" with text / font / bold / italic (format 10).
- Emboss is a combine op (`BooleanOp::Emboss`, one part, `EmbossText` in the spec,
  planePoint / planeNormal = the spot and the way the surface faces there, in the
  result's coordinates). It always uses the mesh engine.
- `emboss::letters` makes the letters' solid on the part's triangles:
  1. `text::flatLetters`: the letter faces triangulated flat, centred, turned, and every
     edge split until none is longer than letterHeight / 25, so letters can bend.
  2. Wrapping, like smoothing on a sticker: a plane through the spot cuts the surface
     along a baseline running "across"; from each step along it a second cut, square to
     the baseline, runs "up". A letter point (u, v) goes u along the baseline, then v up
     its column, both measured along the surface. Cuts are followed triangle to
     neighbouring triangle (edge table of the triangles near the words), so each costs
     only what it crosses. Running off the surface, or words longer than the way round
     a closed shape, is an error.
  Big shapes (over 150,000 triangles) preview the words alone (`previewWords`), drawn
  over the shape; the cut happens on Apply.
  3. Each point stands out along the surface's facing there (corner normals averaged over
     faces bent less than 35°, so box faces stay square). Raised: depth out, a little in;
     engraved: depth in, a little out. Tops, bottoms and outline walls make a closed
     piece per letter piece; pieces are united, then merged into or cut from the part.
- `Document::previewMesh` draws the result before it is made (the Emboss tool's live
  preview); the last preview is kept, so applying doesn't work it out again.

## Edges: rounding, fillets, chamfers, bevels (slice 5e)

- A combine op (`BooleanOp::Edges`, one solid part, `EdgeFinish` in the spec: kind,
  size, angle, picked edges). Worked out by OCCT: `BRepFilletAPI_MakeFillet` for
  Rounding / inside Fillets, `BRepFilletAPI_MakeChamfer` (`Add` for Chamfer, `AddDA` for Bevel:
  width on the picked face, angle to it). A Fillet on an outside edge is a cove:
  `cutCoves` subtracts everything within the radius of the edges (a cylinder along a
  straight edge, a torus segment along an arc, a swept circle otherwise, and a ball at
  each end so corners join). Not for mesh-engine shapes.
- `edges::analyse` lists the edges two faces meet at (no seams), with a polyline each
  and how they bend: Convex (rounding), Concave (fillet), Smooth (nothing to do).
  Concave = walking off the edge across one face heads the way the other face points.
- Picks are stored by place (`EdgeRef`: a point on the edge, a point on the face it was
  picked from), in the result's coordinates, plus (`edges::locate`) where the edge sits
  within the part's box (0..1 per axis) and which way its face points. `edges::match`
  finds each pick again: same place; else same spot in the (resized) box with a face
  facing the same way; else the nearest suitable edge within a fifth of the shape's
  size. Bevels pick their face by its facing. The Edges tool re-opens with
  `pickedEdges`, matched the same way.
- The tool: `Viewport::hovered` gives the ray under the mouse; the window finds the
  suitable edge drawn within 9 px that the shape doesn't hide (`edges::FaceFinder`),
  draws picks and hover as thin tubes, and previews the result via
  `Document::previewMesh` (now for OCCT combines too).

## History: steps, switching off, the rollback bar (slice 6)

- An object's history is read from its result tree: the main line follows each step's
  first part down to the starting shape (`history::steps`, oldest first). Other parts
  are the step's tools and appear under it in the Objects tree.
- Steps can be switched off: `Body::suppressed` (by hand) or `Body::rolledBack` (below
  the bar). Off, a step passes its shape through (`Document::passThrough`: the first
  part for one-part steps and Subtract, all parts side by side for Merge / Intersect /
  Group). Saved in the file (format 12).
- The bar sits below the oldest rolled-back step on the main line (`history::barAt`);
  moving it is one EditBodiesCommand (`history::rolledTo`). While rolled back, Edges
  and Emboss work on the last shown step (`history::tip`) and the new step is put in
  between it and the next one (`Document::insertStep`, InsertStepCommand). Its picks
  are converted into the coordinates of the step that will hold it. Combining,
  mirroring and splitting a rolled-back object are blocked until it's rolled forward.
- A step that can't be built (an earlier change left it nothing to work on) is passed
  through like a switched-off one, and `Document::problemWith` says why; the tree marks
  it. Deleting a step (`RemoveStepCommand`) puts its first part back in its place and
  its tools back on the plate.
- Merge / Subtract / Intersect can be switched into each other in place
  (`Document::setBoolean` allows it).
- Subtract `throughAll`: each straight tool (box, cylinder, tube, text) is rebuilt with
  its height stretched to reach past the first part's box along the tool's own Z
  (`Document::throughTool`), in both engines.
- Emboss centring (`emboss::centeredOnFace`): spread from the triangle at the spot
  across edges bending less than 35° to get the face, take its extent along the words'
  across / up directions, and drop onto the face above the middle.

## Sketches, Pad and Pocket (slice 7)

- `sketch::Sketch` (core, no OpenCASCADE): points (point 0 is the fixed origin), radii,
  geometry (lines, circles, arcs by point index) and constraints (rules and
  dimensions). `sketch::solve` is a damped least-squares (Levenberg–Marquardt) solver
  with a numeric Jacobian; residuals are in millimetres (angles scaled by the sketch's
  size). It reports `freedom` (variables minus the Jacobian's rank) and the rules it
  couldn't meet. While dragging, `pull` adds weak residuals towards the mouse.
  Tangency at a shared end point is written as "square to the radius" (or "centres in
  line"), since the distance form has no slope there.
- `makeRule` makes ready-to-add rules (dimensions take the current measure; Tangent
  picks its side), and the editing helpers keep indices consistent when points are
  joined or geometry removed.
- `sketch::faces` (SketchShape) builds the closed outlines into faces: wires found from
  the edges, nested by containment (even depth solid, odd depth a hole).
  `PadSpec{sketch, length, reversed, midplane}` is a primitive; its solid is the
  prism of those faces. It turns about the sketch origin (`pivotOf` = 0); handles and
  rotation use its box like a combined shape's.
- Sketching on a face: `sketch::planeOfFace` gives the face's frame (origin at its
  centre of area, Z out of the solid, X along the plate's X on up / down faces, else
  level) and its outline. The pad's placement is that frame
  (`placementFromTransform`). On a face, Finish adds the pad and a Merge (Pad) or
  Subtract (Pocket: the pad reversed into the solid, Through all optional) in one
  macro. Changing a sketch later is one EditBodyCommand on the pad (plus the step for
  Through all); `outlinesOnPlane` finds the face under it again to show.
- App: `SketchView` (a QWidget swapped in for the 3D view by a QStackedWidget) draws
  and edits the sketch with its own undo; `SketchPanel` holds the finish options.
  While sketching every other window command is switched off (`lockForSketch`), so
  the sketch keys (L, R, C, A, S, H, V, D, Delete, Ctrl+Z) don't clash.
- File format 13: `"pad"` bodies carry the sketch as JSON.

## Revolve, Hole, Shell (slice 8)

- `RevolveSpec{sketch, axis, angle, reversed, midplane}`: the sketch's faces spun round
  one of its lines (`sketch::revolveShape`, BRepPrimAPI_MakeRevol), refused when the
  outline crosses the axis. Made like a pad (Revolve adds, Groove subtracts on a face).
  On the plate its frame stands the sketch up (sketch Y = plate Z) and lifts it onto
  the plate.
- `HoleSpec{sketch, kind, size, tapped, diameters, depth}`: a tool body whose solid
  (`sketch::holeShape`) is one cutter per loose point of the sketch (points used by no
  curve: `sketch::loosePoints`), each a profile spun round Z, going down from a little
  above the face. A Hole step is a Subtract of it (Through all: `throughTool` makes the
  holes as deep as the part reaches below the face). `holeSizes` holds the ISO metric
  table (clearance medium fit, tap drill, counterbore, countersink).
- `BooleanOp::Shell` + `ShellFinish{thickness, outward, open faces}`: one-part step
  like Edges. Open faces are `FaceRef`s (a point on the face, its normal, and where the
  point sits in the part's box), matched like edge picks: exactly, then by box spot and
  facing, then nearest (`shell::match`). Open faces → BRepOffsetAPI_MakeThickSolid;
  none → the solid minus its inward offset (or the outward offset minus the solid).
- App: `HolePanel` and `ShellPanel` like the Edges panel; the Hole tool previews by
  cutting its cutters from the shape directly. **Place exactly...** opens the hole spots
  in the sketch view (positions only) and returns to the Hole panel.
- `removeGeos` now keeps points that stood on their own; `removePoints` removes points.
- File format 14: `"revolve"` and `"hole"` bodies (with their sketches), `"shell"` steps.

## Middle planes and the ribbon (slice 9)

- `sketch::middlePlane(which, centre)` gives the front (X right, Z up, facing -Y), side
  (Y right, Z up, facing +X) or top frame through a point (the shape's box centre);
  `sketch::sectionOutlines` cuts the shape with that plane (BRepAlgoAPI_Section) for the
  sketch view's reference lines. Finishing works as on a face (Merge / Subtract step);
  when a sketch is changed later and no face lies in its plane, the section is shown.
- The ribbon is a fixed toolbar holding a QTabWidget; each tab is rows of groups of
  QToolButtons that run the window's existing actions (so enabling, menus and
  shortcuts are unchanged). The Sketch button is a split button whose menu holds the
  plane choices.

## Follow (slice 10)

- `BooleanOp::Follow` + `FollowFinish{path sketch, origin / xAxis / normal, relative,
  ridge, profile, width, depth, custom sketch}`: a one-part step like Shell.
- Laying the path (`follow::laidPath`): the sketch's lines and arcs are joined into runs
  and sampled; a walker starts at the origin (on the surface; after resizing, moved to
  the same spot in the part's box and onto the surface) and takes small steps
  (about 1/400 of the part's size) along its own across / ahead directions, each step
  snapped to the nearest point on the part's faces (the face it was on first, else
  BRepExtrema on the faces), its directions carried over to the new tangent plane.
  On a flat face this reproduces the drawing exactly; on a cylinder it wraps without
  stretching.
- Sweeping (`follow::swept`): each run becomes a wire of B-spline edges (one per drawn
  curve); a second wire 1 mm out along the surface normals guides
  BRepOffsetAPI_MakePipeShell, so the cross-section stays upright to the surface.
  Corners use round transitions. The swept solid is cut from (groove) or fused to
  (ridge) the part. Cross-sections reach a little past the surface so the result is
  clean.
- App: `FollowPanel`; the path and the cross-section are drawn in the sketch view
  (opened from the panel, like the Hole tool's **Place exactly**). The preview shows
  the result and the laid path as orange tubes.
- File format 15: `"follow"` steps.

## View cube (slice 20)

- `ViewCube` (src/app): pure geometry and drawing. Six faces (Front = -Y, Z up), each
  split 3 × 3 into pieces whose look-from direction is the face normal plus the
  piece's offset (so edges and corners give 45° and three-quarter views); only faces
  turned towards the viewer are drawn and hit-tested, in an orthographic projection of
  the view matrix's rotation. Labels are drawn with a QTransform mapping the face's
  square onto its outline.
- `Viewport` paints it with QPainter after the GL scene, takes left clicks on it (and
  on the three buttons under it) before anything else, and turns the camera with a
  QVariantAnimation over `OrbitCamera::setAngles` (the short way round in yaw).

## Sculpting (slices 17–23)

- `sculpt::welded` (shared corners), `sculpt::refined` (edges split at their middles
  until short enough; conforming, so a closed mesh stays closed) and `sculpt::Sculptor`
  (core, `Sculpt.h`): point neighbours and point→triangle lists in compressed rows;
  `hit` (brute-force Möller–Trumbore over all triangles: ~20 ms at 500k, the first
  thing to speed up with a BVH); `apply(Dab)` walks out from the triangle under the
  brush through neighbours inside the ball, moves those points (Draw: along the
  falloff-weighted average normal, 0.08 × radius × strength at the middle; Smooth: 4
  rounds of Laplacian), then recomputes normals for them and their neighbours.
- Brushes (slice 18): Inflate (each point along its own normal), Flatten (towards the
  falloff-weighted plane), Pinch (towards the centre, along the surface), Clay (points
  under a plane just above the area rise to it), Grab (`beginGrab` takes the points and
  weights, `moveGrab(offset)` sets each to its start + offset × weight; the app moves
  the offset across a plane through the grab point facing the view). `Symmetry` mirrors
  dabs and grabs across planes through the shape's middle; images start from
  `nearestPoint` (brute force).
- Detail and picking (slice 19): the Sculptor keeps per-point triangle lists (vectors,
  so triangles can be added) and a hashed grid of triangles (cells about twice the mean
  edge; each triangle in every cell its box reaches) walked with Amanatides–Woo for
  `hit`. Moved points move their triangles between cells. `setDetail(finest)`: before
  a dab, `splitAround` splits edges longer than max(finest, radius × 0.08) round the
  touched points (red-green, as `refined`, conforming across the region's edge), the
  first piece keeping the triangle's slot. `topologyVersion()` tells the app to
  re-upload the whole mesh (`Viewport::meshGrew`) instead of a point range; Undo stroke
  keeps whole meshes.
- Mask, Crease, Snake hook (slice 22): the mask is `Mesh::shade` (one float per point,
  0..1, empty when none; split points take the average of their edge's ends). Brush
  moves are scaled by 1 − mask (Smooth per round, Grab and Snake per point). The
  viewport always uploads a shade buffer (attribute 2, zeros when none) and darkens by
  it; point-range updates write it too. Crease: Draw inwards with falloff² plus a pinch
  towards the centre. Snake hook (`beginSnake`/`moveSnake`): per image a tip and the
  points pulled last time; each move splits stretched edges round the tip, then moves
  the points by (to − tip) × falloff × (0.5 + 0.5 × strength) and the tip follows by
  the same factor. Kept as strokes: Mask dabs; MaskClear / MaskInvert with no dabs;
  Snake hook dabs are the tips in turn (the app takes each from a plane through the
  last one, facing the view). File format 19.
- Remesh and Simplify (slice 23, `Remesh.h`): both work on a `Work` mesh (triangles and
  points that can die or be added, per-point triangle lists, open-edge points found
  once and never touched). `remeshed` (Botsch–Kobbelt): split edges > 4/3 L, collapse
  < 4/5 L to the middle (link condition, no fold-over, no edge > 4/3 L), flip towards
  valence 6, slide points to their neighbours' middle along the surface, then put each
  on the nearest point of the original (a grid of its triangles; the mask is
  interpolated from there); 4 rounds. `simplified` (Garland–Heckbert): welds, then
  collapses edges by least quadric error (optimal point, else the better end or the
  middle) from a heap with per-point stamps until the count is reached. Sculpt brush
  `Remesh` is a stroke with the size as radius (`Sculptor::remesh`). `BooleanOp::Simplify`
  (`SimplifyFinish{keep}`) is a mesh-engine one-part step; the app refuses shapes under
  2,000 triangles. File format 20 (and `kLastSculptBrush`, checked against the enum).
- Pattern brush (slice 24, `Patterns.cpp`): built-in patterns are 256-wide grey pictures
  made in code (two repeats of the feature across, seamless, stretched to full range,
  made once); a user's is a `PictureData`. `SculptFinish::patterns` holds those a step
  uses; a stroke has `pattern` (its number) and `tile` (mm per repeat). Between
  `beginPattern`/`endPattern` the Sculptor keeps, per point, where it was, its normal
  and the most weight it has had: a dab sets it to start + normal × depth × height × w
  only where w (falloff × (1 − mask)) beats that, so overlaps don't deepen. The
  picture lies flat across the first dab's facing (u = n × Z, or × X near vertical),
  sampled bilinearly with wrap at start·u / tile; mirror images sample their point
  mirrored back. Detail under it: down to tile / 24. Depth = ±strength × tile × 0.3.
  File format 21.
- The display item points at the sculptor's mesh (aliasing shared_ptr), and
  `Viewport::meshPointsChanged` uploads just the changed range with glBufferSubData.
- Sculpt steps (slice 21): `BooleanOp::Sculpt`, one operand, `SculptFinish{detail,
  strokes}`; each `SculptStroke` keeps brush, invert, radius, strength, mirrors, finest
  detail, its dab centres (floats) and, for Grab, the final offset — all in the part's
  own coordinates, so moving or turning the part carries them with it. A mesh-engine
  op: `Document::sculptStart(part, detail)` is the part's surface (mesh-engine
  triangles, or the B-rep tessellated finely), unplaced, welded and divided to the
  Detail's triangle count (`sculpt::startingMesh`); `sculpt::replayed` plays every
  stroke on it (each dab starts from `triangleAt(centre)`, the mirror planes through
  the start's middle) and the result is placed by the part's `localTransform`. Change
  a size below and the strokes play again on the new surface; scaling maps the
  strokes through the part's old and new placements. The app sculpts in the part's
  coordinates (`worldTransform(part)` shows it) exactly as replay does (centres and offsets rounded to float,
  seeds from `triangleAt`), so live and replayed meshes are identical; Done hands the
  live result to `Document::rememberSculpt` (the last 4, keyed by starting surface and
  strokes) so making or undoing the step doesn't play it again. Editing a step replays
  its strokes and appends new ones (Detail fixed). File format 18: `"sculpt"` op.

## Releases (slice 16)

- `installer/Forge3D.iss` (Inno Setup 6): per-user or all-users install, Start menu,
  optional desktop icon, `.f3d` association (HKA, so it follows the install mode). The
  Windows build makes it and a portable zip after packaging, and on a `v*` tag publishes
  them with `gh release create` (notes from `docs/release-notes/<tag>.md`). See
  docs/RELEASING.md.
- `FORGE_RELEASE_LABEL` (top-level CMakeLists) → `build::kReleaseLabel`; shown after
  the version.
- `updates::newerRelease` (core, tested) reads GitHub's release list; the app fetches it
  with QNetworkAccessManager (Qt Network; on Windows the schannel TLS plugin is
  packaged). Preferences: `updates/atStart`.
- The app icon: `src/app/appicon/forge3d.svg` (window icon, through `icons.qrc`) and
  `forge3d.ico` (the exe, through `Forge3D.rc`).

## Quick start (slice 15)

- `tools/make_quick_start.py` writes the guide twice from one text:
  `docs/QUICK_START.md` (GitHub) and `src/app/help/quick-start.html` (in `help.qrc`
  with the screenshots from `docs/quick-start/`), shown in a QTextBrowser dialog.
  Re-run it after changing the guide.
- `Tour` (src/app/Tour.*): a mouse-transparent overlay that dims the window except the
  target, plus a note (QFrame) placed below / left / right / above the target. Steps
  find ribbon buttons with `MainWindow::ribbonButtonFor(action, tab)`. The first run
  starts it (QSettings `tour/seen`).
- The Properties dock is a QScrollArea; PropertiesPanel's hidden page is size-policy
  Ignored so only the shown page sets its height.

## Icons (slice 14)

- `src/app/icons/*.svg` (24 × 24, 1.6 px strokes, `#3a3f45` lines, `#2b78d0` accent),
  listed in `icons.qrc` (AUTORCC). `ribbonIcon(name)` renders one with QSvgRenderer at
  16–64 px into a QIcon (so Qt's SVG icon plugin isn't needed), swapping the line colour
  for a light one when the window palette is dark. Actions carry the icons; ribbon
  buttons are `ToolButtonTextUnderIcon` at 32 px.

## Phase 1 polish (slice 13)

- `faceUnder` / `laidOnFace` (Arrange): the clicked point and facing from the shape's
  display mesh (`emboss::surfaceAt` with a tiny radius, so a flat face's own facing);
  the turn taking it to -Z (Rodrigues, about the shape's visible centre), then down so
  the lowest mesh point is at Z = 0; back to a placement with `placementFromTransform`.
- Ribbon pages sit in `RibbonScroll` (a QScrollArea as tall as its buttons, plus a
  scroll bar only when the window is too narrow). Undo / Redo ribbon buttons keep
  short text; the full label is the tooltip.
- Objects double-clicks are handled in `eventFilter` (MouseButtonDblClick): the first
  click's selection rebuilds the tree, and QAbstractItemView then drops the
  double-click because its pressed index is gone.

## Emboss pictures (slice 12)

- `EmbossPicture` (in `BooleanSpec::picture`, instead of `emboss`): the name, a shared
  `PictureData` (grey bytes, at most `picture::kMaxSide` = 1000 px; the app converts any
  picture with `picture::fromRgba`, transparent as white; SVG drawn by QSvgRenderer
  first), logo or relief, threshold, invert, width, depth, raised, angle.
- `picture::flatPieces` turns it into `text::FlatPiece`s, the same flat triangles words
  use. Logo: `picture::outlines` (marching squares at the threshold, so outlines run
  smoothly between pixels; Douglas–Peucker to 0.35 px; specks under 4 px² dropped; at
  most 5000 loops / 200000 points), nested by containment into OCCT faces with holes,
  meshed by `text::flatPieces`. Relief: one sheet of at most 200 squares along the long
  side, each point carrying a height share (`FlatPiece::heights`, the shade averaged
  over its square).
- `emboss::onSurface` (was the body of `letters`) lays any pieces on the surface and
  makes them solid; a point with a height share stands out (or sinks) that share of the
  depth (at least 0.02 mm). `emboss::pictureSolid` is the picture's way in. The walks
  across the surface now go the whole span each way, so a loop round the shape is
  always noticed.
- File format 17: `"picture"` with `"image": {width, height, grey}`, grey deflated
  (miniz) and base64, inside document.json.

### Follow: checks (0.20.2)

- `follow::apply` cuts / fuses with a fuzzy value (1e-5 × the part's box diagonal): a
  round groove's tube is centred on the surface, and without it some cuts removed
  nothing. The result must be valid, have one shell per solid, and lose (groove) or
  gain (ridge) volume, else `GeometryError`.
- `checkNotOverItself` (in `swept`, so patterns get it too): a run that comes back
  within 0.9 × the cross-section's width of itself, more than 1.6 × the width further
  along (cyclic if closed), is refused.

## Patterns (slice 11)

- `BooleanOp::Pattern` + `PatternFinish{kind, feature, directions, counts, spacings,
  centre, axis, count, angle}`: a one-part step. `pattern::placements` gives a
  transform per copy (the first the identity); circles turn about a line (Rodrigues).
- The whole part (`feature == false`): a compound of the part placed by each transform
  (like Group; meshes: `meshkernel::together`).
- Its last feature (`feature == true`): the operand must be a Merge / Subtract of two
  parts (its tool, through all stretched as the step does) or a Follow (its swept
  solid, recomputed); the copies of the tool are fused to / cut from the step's result
  in one boolean (OCCT SetArguments / SetTools; meshes: merged / subtracted). Because it
  reads the feature's tool every time, the copies follow any change to it.
  The boolean runs with a fuzzy value (1e-5 × the part's box diagonal) and in
  parallel: without it, overlapping or nearly touching curved copies (a Follow groove
  round a cylinder) took minutes and came out invalid. A `Message_ProgressIndicator`
  (`Deadline`) stops it after 30 s, and a `BRepCheck_Analyzer` check refuses an
  invalid result; both throw `GeometryError`, so the step is marked ⚠.
- Directions and centre are kept in step coordinates (moved like the other steps'
  picks); scaling stretches each spacing by the scale along its direction.
- App: `PatternPanel` (lists with their labels above), preview via `previewMesh`.
- File format 16: `"pattern"` steps.

## Rendering

`Document::mesh()` → `forge::Mesh` (positions, normals, indices) → `DisplayItem`
(mesh + model matrix + colour) → `Viewport::setItems()` → one GPU buffer set per body →
drawn each frame.

Tessellation gives each B-rep face its own vertices, so normals are smooth within a face
and sharp across edges.

## Conventions

- Namespaces: `forge` (core), `forge::primitives`, `forge::view`, `forge::app`.
- Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.
- Log with `qInfo()`, `qWarning()`, `qCritical()` in app code; they go to the log file.
- Each delivered slice gets a test sheet in `docs/test-sheets/` and a `CHANGELOG.md` entry.

## Measure (slice 25)

- `measure::Shape` (core, `Measure.h`) prepares a placed solid (`pickableShape`): its
  real edges (two faces; no seams) as polylines, their end corners, a `FaceFinder`, and
  face triangles on demand. A `Target` is a face, an edge or a point (plate
  coordinates). `describe` reads the OCCT geometry (BRepAdaptor: line, circle/arc;
  plane, cylinder, cone, sphere, torus; lengths and areas by GCPnts / BRepGProp).
  `between` takes the nearest points (BRepExtrema_DistShapeShape; a point is a vertex),
  the acute angle between directions (a line against a face measured from the face),
  how far apart parallel faces / lines lie, centre to centre (points, or parallel
  axes), and a centre or face-parallel axis to a flat face.
- The app: `pick` finds the shape under the mouse; then a corner within 10 px, else an
  edge within 8 px (not hidden behind the shape), else the face; Shift or a mesh shape:
  the exact point. Shapes are prepared once per Measure session (cleared on any
  document change). Highlights are ghost items (orange hover, red picks, green nearest
  line, blue centre line); the panel shows rich text.

## Threads (slice 26)

- `BooleanOp::Thread`, one operand, `ThreadFinish{face (a FaceRef, as Shell's open
  faces), size, pitch, fit, leftHand}` in step coordinates (`threadSpec`,
  `threadInPlate`, `withThread`; `pickableShape` gives the operand, as for Shell).
- `thread::roundFace` reads the cylindrical face: radius, axis, its extent along the
  axis (UV bounds) and whether it's a hole (the oriented normal points at the axis).
  `suggest` picks the nearest ISO coarse size (a hole by D − P, the tap drill).
- `thread::cutFor` makes closed triangle meshes: a peg's crest at min(R, D/2 − fit)
  (a tube clears the peg down to it if bigger), a hole's at max(R, D/2 − 0.5413 P +
  fit) (a bar opens it up); the groove is a four-sided section (60° flanks, 0.6134 P
  deep, P/8 flats) swept round a helix, 64 steps a turn, run a pitch past both ends and
  trimmed (Manifold intersection) to the face's length. The step subtracts them from
  the part with the mesh engine. File format 22.

## Loft and sweep (slice 27)

- Two more `PrimitiveSpec` kinds made from sketches (before `MeshSpec`, so
  `kPrimitiveKindCount` is size − 6): `LoftSpec{sections (sketch, height), ruled}` and
  `SweepSpec{profile, spiral, path, pitch, turns, leftHand}`. They have no size
  fields: `halfExtents` reads the shape's box and `setSize` / scaling are even only
  (`scaled()` scales the sketches, heights and pitch).
- `loftShape`: each section's outer wire, raised to its height (heights must
  increase; one closed shape each), through `BRepOffsetAPI_ThruSections` (solid,
  ruled or smooth).
- `sweepShape`: `BRepOffsetAPI_MakePipeShell` with the profile's face wire. A drawn
  path is joined into one wire, turned round if drawn from the far end (it must start
  at the sketch origin, where the outline is), and stood up (sketch Y → Z); corrected
  Frenet off, round-corner transitions. A spiral is a 2D line on a
  `Geom_CylindricalSurface` round the sketch's Y axis (radius = the profile's centre),
  swept with a fixed binormal (`SetMode(gp::DY())`); the profile must sit on one side
  of the axis and the pitch must exceed its height. On the plate a spiral uses the
  upright transform, as Revolve does.
- The app: `SketchMode` keeps the loft/sweep being drawn; `storeSketchStage` saves
  the sketch on screen into the current outline or the profile/path before switching
  (`loftStep`, `sweepStage`); neighbouring outlines (or the profile edge-on and an
  upright guide) are grey reference lines. File format 23.

## Sketch tools (slice 28)

- `SketchEdit.h` (core): `roundCorner`, `trimPiece` / `trim`, `offset`, `mirror`. Each
  edits the `Sketch` and adds the rules that hold the result; they throw
  `std::invalid_argument` with a user message and change nothing when they can't.
- Round corner: the two lines are shortened to new points, an arc joins them (two
  Tangent rules and a Radius dimension), and the corner point stays on two helper
  lines Parallel to the lines (sharing their new ends, so in line with them). A
  length on either line becomes a Distance from its far end to the corner; Equal
  rules on them are dropped.
- Trim: `cutsOf` finds where other curves cross a curve (line/line, line/circle,
  circle/circle, within both curves); the piece between the cuts either side of the
  click goes. Ends: the end point is replaced (PointOn the cutting curve, or an
  existing point there). Middle: a second curve is added (lines: Parallel + PointOn;
  arcs: the same centre + Equal). Circles become arcs. Lengths on a cut line go.
- Offset: `chainOf` walks the joined lines and arcs (no branching); each joint moves
  to where the two offset curves meet (smooth joints: straight out); closed chains
  use the signed area to know which way is out. Copies: lines Parallel to the
  originals, arcs on the same centre point, Tangent rules copied, and a `Rule::Gap`
  dimension (two lines: the copy's start a signed distance left of the original;
  circles / arcs: the difference in radius) for each run of pieces joined by Tangent
  rules (a run shares one gap). File format 24.
- Mirror: each point gets an image held by a Symmetric rule (points on the axis are
  shared); arcs swap start and end; circles get an Equal rule. `endsOntoLine` first
  moves loose ends within 3% of the picked curves' size onto the axis (PointOn), and
  the app calls `straighten` (an axis within 10° of upright / level is turned to it,
  its PointOn points with it, and gets a Vertical / Horizontal rule) when the rules
  allow. Both place the points directly: left to the solver, a PointOn pull can send
  the line's free end far away.
- The app: `SketchView::change` wraps a tool (undo snapshot, solve, put back with the
  reason on failure). Trim is a tool mode (`Tool::Trim`, hover shows `trimPiece`);
  the others act on the picks and ask for a size.

## Taper (slice 29)

- `PadSpec::taper` (degrees; 0 keeps the old straight path through `padShape`
  untouched). Each sketch face is pushed from the sketch plane (`taperedPrism`) and its
  side faces leaned with `BRepOffsetAPI_DraftAngle` (neutral plane: the sketch plane;
  pull: the way it's pushed), so planes stay planes and cylinders become cones.
  Midplane: two halves from the sketch plane, fused.
- Too steep is caught first in 2D: the face offset inward by length × tan(taper)
  (`BRepOffsetAPI_MakeOffset`) must keep as many wires as the face has.
- Through-all pockets drop the taper (their tool is rebuilt from below the part).
  Scaling keeps the sides' slope (tan × across / along); uneven across isn't allowed.

## Text in sketches (slice 30)

- `Sketch::texts` (`Words`: text, font, bold, italic, capital height, angle, and `at`,
  a sketch point for the left end of the baseline). The point is an ordinary solver
  point; the letters themselves aren't in the solver. Point renumbering
  (`mergePoints`), `pointsInUse` (so the point isn't a loose point / hole spot),
  `removePoints`, `scale` (even only) and `checkRefs` know about them.
- `wordsFaces` (SketchShape) builds the letters with `text::letterFaces` (em size =
  height / capitalRatio), turned and moved to the point; `faces()` adds their wires to
  the sketch's loops, so the usual even-odd nesting makes letters solid on their own
  and holes inside an outline. Saved as the sketch's `"texts"` (file format 24).
- The app: `Tool::Text` asks with a small dialog; `Pick::Text` picks words by their
  box; outlines for drawing come from `wordsOutlines` (at the origin, unturned) and
  are kept per text / font / style / height in `m_lettering`.

## Section view (slice 31)

- View only: `Viewport::setSection({normal, offset})`. The mesh fragment shader
  discards fragments with dot(normal, world position) > offset (`uModel` gives the
  world position) and, while cutting, draws back-facing fragments (the inside of a
  closed solid seen through the cut) in a flat darker shade lit as the cut plane: the
  cut reads as a filled face without building any geometry. Nothing in the document or
  picking changes.
- `Section::selectedOnly`: the viewport remembers the ids of the selected items
  (`noteSectionShapes`, on every `setItems`; an empty selection keeps the last ones)
  and switches `uClipOn` per item. `sceneBounds(cutOnly)` gives the slider its reach.
- `SectionPanel` in its own dock (hidden until View → Section View); the dock's
  visibility is the on/off switch. `MainWindow::applySection` sets the slider's reach
  from `Viewport::sceneBounds()` when it opens or the axis changes.

## Recent files, plate size, ribbon (slice 32)

- Recent files: `file/recent` in Preferences (native separators, newest first, six),
  noted by `openFile` and `saveTo`; `noteRecentFile({})` rebuilds the menu.
- Plate: `Viewport::setPlateSize` (grid lines counted from the middle, an edge line;
  rebuilt in `paintGL` when dirty); `plate/x`, `plate/y` in Preferences (an app
  setting, not saved in models).
- Ribbon: five tabs (Build, Design, Sculpt, View, Print/Save); `common()` adds Check
  and Undo to each. `Viewport::setLeftDrag` makes the left button orbit, pan or zoom
  (an `ExclusiveOptional` action group: one or none); the Section dock follows
  `m_sectionAction`.

## Fit warning (slice 33)

- `Viewport::checkFit` (on `setItems` and plate changes): an item whose box corners
  all sit inside the plate and under `m_plateZ` fits; otherwise its points are tested
  (a turned shape's box overshoots). Ids that don't fit go in `m_offPlate`; the mesh
  shader's `uWarn` tints them and `offPlateChanged(count)` feeds the status bar.
  `plate/z` and `plate/warn` in Preferences.

## Backup copies (slice 34)

- Each session has `<AppLocalData>/recovery/<time>-<pid>.f3d`, with a `.json` beside
  it (the model's own path, when it was written) and a `QLockFile` (`.lock`) held for
  the session's life. `autosaveNow` (a 2-minute timer; `FORGE3D_AUTOSAVE_SECONDS`
  shortens it for testing) writes only when the stack isn't clean and the document
  changed since the last backup (`m_docSerial`), to a `.part` file swapped in.
- `removeRecovery` on save, open, new and a normal close. At start `offerRecovery`
  lists backups whose lock can be taken (their session is dead) and asks;
  `recoverFrom` loads one and marks the stack modified (`CommandStack::setModified`).

## Slice 35

- Backups: `backup/on`, `backup/minutes` (2..60) in Preferences; `chooseBackups`
  re-applies them to the timer.
- `pick(doc, ray, const PickClip*)` skips hits on a section's hidden side (all bodies,
  or `only`); `MainWindow::pickShown` builds the clip from `Viewport::section()` and
  is what selecting, dragging, Lay on Face and Measure's body pick use.
- `Viewport::setLabels` draws lines with a boxed text at the middle (QPainter, after
  the GL scene). `m_measureLabels` (the live measurement) and `m_keptLabels` (Keep on
  Screen; cleared by the document listener on any change).

## Sketch on the section's cut (slice 36)

- `sketchOnPlane(which, atSection)`: the middle-plane sketch with the plane moved to
  the section's position along its axis (`sketch::middlePlane` at that point;
  `sectionOutlines` gives the reference). `otherWay` is preset when the plane's normal
  points at the hidden side.
- In `sketchFaceClick`, a ray that meets the section plane coming from the hidden side
  at a point inside the shape (`BRepClass3d_SolidClassifier`) is a click on the cut
  face and goes to `sketchOnSection`.

## Theme (slice 37)

- `Theme.h/.cpp`: `theme::apply(app, choice)` sets Fusion with a dark palette for
  Dark (Follow Windows reads `AppsUseLightTheme` in the registry), and the starting
  style and palette for Light (Fusion's light palette if the computer started us dark).
  `theme::look()` gives the colours Forge3D paints itself: the 3D view's background and
  grid, the sketcher's paper, lines and labels, the view cube, the tour note.
- `MainWindow::chooseTheme` switches live: icons are drawn again from their SVGs
  (`setIconNamed` keeps the action → name list), style-sheeted widgets are polished
  again, and the viewport rebuilds its plate lines. `ui/theme` in Preferences.

## Split for printing (slice 38)

- `PrintSplit.h` (core). `sectionLoops` slices the body's display mesh with the plane
  (crossing points computed from a canonical edge order, joined end to end), so it
  works for solids and meshes alike; `sectionArea` uses even-odd nesting.
- `keySpots`: a grid over the cut, kept where it's inside (even-odd) and at least
  r + clearance + 1.2 mm from every edge; grouped by the smallest loop holding it
  (one group per piece of the cut); in each, the roomiest spot, then the furthest
  from those chosen (2, or 3 over 4000 mm²), each checked by a ray through the mesh
  for room for its hole. Diameters ×0.75 and ×0.5 are tried before giving up.
- `splitWithKeys`: `splitBody` (Both), then `ConeSpec` bodies placed along the
  normal: pegs (frustums, embedded 0.3 mm) merged into the back piece, holes (radius
  + clearance, 0.5 mm deeper, same taper) subtracted from the front one; loose pins
  are two cones merged, standing beside the model. All in one macro.
- `smartSplit`: while a piece doesn't fit the volume (X/Y may swap; room along the cut
  axis is reduced by the peg length), cut it across its most-overflowing axis at the
  sampled position (14 samples in the window that leaves a fitting first piece and a
  cuttable rest) with the smallest section area, lightly weighted towards even
  spacing. `planSmartSplit` counts pieces from the box for the confirmation.
- Viewport fit warning: `m_offPlate` holds 2 (too big) or 1 (off the plate, would fit
  if moved; X/Y may swap); `uWarn` 1.0 / 0.5.

## Robustness (0.47.1 audit)

- `SafeApplication` (main.cpp) overrides `QApplication::notify`: a `std::exception`,
  OCCT `Standard_Failure` (not a `std::exception` in OCCT 7) or anything else thrown
  from an event handler is logged and reported in one deferred message box instead of
  ending the process.
- `Document::shape` / `mesh` turn `Standard_Failure` into `GeometryError`, so callers
  only need `catch (const std::exception&)`.
- Input bounds live in core, where both the UI and files go through them:
  `primitives::requirePositive` (1e-4 … 1e6 mm), `text::kMaxLetters` (120),
  `edges::apply` (size ≤ the shape's largest box side), `thread::cutFor` (≤ 2000
  turns), 3MF corner count (counted with memoised component expansion before
  building, 90 M corners), `repairMesh` (finite, |v| ≤ 1e7).
- Files: `fromJson` refuses any number that is non-finite or over 1e7 in size;
  `sketch::damage()` checks every curve's and rule's indices (and that direction rules
  are on curves with two points) after a sketch is read.
- `MainWindow::refreshScene` catches per body: an unbuildable shape is skipped (and
  named in the status bar) rather than ending the loop.
- `tests/fuzz_files.cpp` (hidden tag `[.fuzz]`): reads saved documents from
  `FORGE3D_FUZZ_CORPUS`, checks each still opens, then replaces leaves with odd values
  (`FORGE3D_FUZZ_PER_FILE` per file) and opens and builds every body.
  `FORGE3D_FUZZ_TRACE` names a file that holds the case in hand, for finding hangs.

## Colours (slice 39)

- `Body::colour` (optional 0xRRGGBB); `Document::colourOf` walks first operands until
  a body has one, else `kDefaultColours[(id - 1) % 6]` (the old display palette, now in
  core). `Document::setColour` notifies Modified without invalidating the shape cache.
- `SetColourCommand(ids, colour)`: one undo step for several objects. The app colours
  `history::objectOf` of each selected id (`selectedObjects`).
- File: `"colour": "#RRGGBB"` on a body, only when set (a string, so the reader's
  1e7 number limit doesn't apply). No format-version change: older readers ignore it.
- 3MF export: `ExportMesh::colour`; one `<basematerials>` (id after the objects) with a
  `<base displaycolor="#RRGGBBFF">` per distinct colour; objects carry `pid` / `pindex`.
  Import reads `basematerials` and an object's `pid` / `pindex` (or its first
  triangle's `pid` / `p1`, or a coloured component's) into `RawMesh::colour`.
- UI: `m_colourMenu` (Build → Look), also a submenu in the Objects context menu; a
  `QWidgetAction` grid of `kSwatches` buttons, More Colours… (`QColorDialog`) and
  Automatic. Each choice is applied on a zero-length timer, after the menu has closed.

## Smooth, Extrude Down, looks (slice 40)

- `BooleanOp::Smooth` (`SmoothFinish::strength` 1..10) and `BooleanOp::ExtrudeDown`
  (`planePoint` / `planeNormal`: the plate and "up", in the step's coordinates, set when
  the step is made) are mesh-engine steps. File format 25 adds `"smooth"` /
  `"extrudeDown"`.
- `meshsteps::smoothed` (MeshSteps.cpp): under 150k triangles, remesh to an edge of
  min(diag/60, mean edge) first; then uniform-Laplacian steps (λ 0.5), about
  (reach / edge)² of them (reach = strength × diag / 60; at most 300), open-edge points
  pinned, in chunks of 12, remeshed again between chunks (points drift and bunch up,
  which made lumps); then scaled about the centroid back to the starting volume.
  Points are welded by position first.
- `meshsteps::columnsBelow`: triangles facing down (against up) and above the plane are
  grouped into connected patches. Each patch becomes one closed solid (`solidBelow`):
  the patch turned over on top (lifted a hair), its projection a hair under the plane,
  and walls round its rim. Corners are merged through shared patch edges (union-find),
  so patches touching at a single point don't pinch. Patches over 40k triangles are
  simplified first, with the top lifted by 2× their mean edge. Solids with well-separated
  boxes are packed into one mesh (`packed`), so a curved underside's many small patches
  merge in a few batches. The step is `merged({part, solids…})`.
- `Viewport::Look` (Solid / SeeThrough / Wireframe): see-through draws normal shapes in
  the translucent pass at alpha 0.42, back faces then front faces (face culling);
  wireframe uses `glPolygonMode(GL_LINE)` from `QOpenGLFunctions_3_3_Core` (not on
  OpenGL ES) and an unlit `uWire` path in the shader. `paintGL` now re-enables depth
  testing (and resets the depth mask, blending and culling) every frame, because the
  QPainter overlays leave it off.
- Saved results: `file::save` writes `results/<id>.bin` (magic `F3DRESU1`, a key, then
  `meshToBytes`) for each top-level mesh-engine shape whose solid is already cached and
  that has no known problem in its tree. `file::resultKey` is FNV-1a over
  `kResultsVersion` and the JSON of the shape and everything it's made from (name and
  colour left out, and the top's placement). `file::useSavedResults` (called after
  `replaceAll` on open and recover) checks each key and `seedSolid`s the matches. Bump
  `kResultsVersion` when any mesh-engine step's result changes. `addStepRows` uses
  `knownProblem` rather than `problemWith` for an object whose result is cached, so
  listing steps doesn't build them.

## PLY and glTF import (slice 41)

- `ImportPlyGltf.cpp`: `readPly` parses the header into elements and properties
  (scalars and lists, all eight PLY number kinds), then reads the body as text or
  binary (either byte order), skipping elements it doesn't use. Element counts are
  checked against the bytes left before anything is reserved. Polygons become
  triangle fans; vertex colours are averaged into `RawMesh::colour`.
- `readGltf`: GLB chunks (JSON, BIN), or a `.gltf` whose buffers are `data:` base64 URIs
  or files beside it (percent-decoded). It walks the default scene's node tree
  (matrix or TRS, column-major), one `RawMesh` per node with a mesh (primitives in modes
  4 / 5 / 6 combined). Positions go through the node's world matrix, then
  (x, −z, y) × 1000 (Y-up metres to Z-up mm); a negative determinant reverses the
  winding. Accessors are bounds-checked against their views and buffers; sparse
  accessors and the Draco / meshopt extensions are refused. `baseColorFactor` is
  converted from linear to sRGB for the colour.
- `importFile` skips the "saved in metres?" question for .glb / .gltf as for .3mf.

## Split to Fit, planned (slice 42)

- `FitCut` {axis, position (where it crosses the middle of the model's box), tiltAxis,
  tilt°}, and `planeOf(cut, box)`. `planFitCuts` takes its per-axis counts (and the
  X/Y turn) from `planSmartSplit`, then places each axis's cuts in order. Each cut is
  sampled at the even spacing and 14 places along the allowed range (and, when angled,
  ±10/20/30° around both other axes, near the best level place). The cost is the
  section area (from a mesh slice of the whole model, its points transformed once),
  plus small penalties for distance from even spacing and for leaning. A candidate must
  leave this piece and the rest able to fit: `spanAlong` gives a plane's lowest and
  highest value along the axis over the box's four edges, so the check errs safe.
- `fitProblems` uses the same spans to report pieces that could be too long, cuts that
  cross inside the box, and cuts that miss it. `splitAlong` applies every cut to every
  current piece whose section it has area in (`splitWithKeys`, one macro), passing the
  other planes as `keepClear`. `keySpots` drops candidates whose hole (along its whole
  depth, both ways for pins) comes within the needed radius of those planes.
  `smartSplit` is now `splitAlong(planFitCuts(..., false))`.
- App: `FitMode` (target, plan, selected cut, the box the plan was made from) and
  `FitPanel` (in the Properties dock). The planes are helper `DisplayItem`s (ids from
  `~0 - 100` down). Pressing on one picks it and starts a `Drag::Kind::FitPlane` along
  the cut's axis. `DisplayItem::helper` keeps tool planes and arrows out of the fit
  warning and the Fit view.

## Lay out (slice 43)

- `Layout.h/.cpp`: plate k (0-based) is centred at x = k × (plate width + `kPlateGap`
  20 mm). `plateHolding(box, plate)` gives the plate a box lies wholly on, or -1.
- `flattestSide`: triangles grouped by facing direction (rounded to 1/60); the 12
  largest groups, plus the way it sits now, are scored by `contactArea`, the area of
  triangles lying flat within 0.05 mm of the lowest level with that side down. Fitting
  the printer comes first, `favour` directions (±, within 5°) count triple, and the
  way it sits now must be beaten by half as much again.
- `layOut` (one macro): `laidOnFace(flattestSide)` (or `sitOnPlate`), a quarter turn
  if that's the only way it fits or it puts the long side along X, then shelf packing
  (sorted by depth, then width) with a 2 mm plate margin and `gap` between pieces,
  starting a new plate when a row won't fit behind the last.
- `CommandStack::abortMacro()` undoes and drops the innermost open macro. Every "on
  failure, endMacro then undo" now uses it, so a failure nested inside another macro
  (or before anything was done) can't undo an earlier step.
- App: `refreshScene` sets `Viewport::setPlateCount` from the highest plate holding a
  shape; `buildPlateGeometry` draws plates 2+ (edges and 50 mm lines); `checkFit`
  judges each shape against the plate nearest its middle. Export groups shapes by plate
  and can write one file per plate, shifted back by `plateCentreX`.


## Sculpt engine (slice 44)

- `sculpt::spatialOrder(Mesh&)`: points sorted by a 63-bit Morton (Z-order) key of
  their position in the mesh's box, triangles by their lowest point. The `Sculptor`
  constructor and `setMesh` call it, so a brush's points are close together in memory
  and in the GPU buffers. Saved results (`kResultsVersion` 2) are from meshes in this
  order.
- Change tracking: `noteChanged(points)` / `noteTriangle(t)` mark blocks of 256
  (`kBlockShift` 8). `takeChanges()` returns them as merged runs `[first, last)`, or
  `everything` after a rebuild (new sculptor, `setMesh`, remesh, mask clear/invert).
  `normalsAround`, the mask brush and `splitAround` report what they change.
- The grid that finds the surface: `gridUpdate` no longer refiles moved triangles; it
  puts them on `m_stale` (`m_isStale` keeps each once). `hit` and `triangleAt` test the
  stale triangles directly as well as the grid. `settle(most)` refiles the oldest
  `most` (only those whose cells changed): all on button release, a quarter of them on
  each dab once more than 100,000 are waiting.
- `parallelFor(n, work)` splits loops of 4,096 or more over up to 8 threads: the Draw
  and Inflate brush loops and `normalsOf`.
- App: `MainWindow::sculptShown` passes `takeChanges()` to `Viewport::meshChanged(id,
  MeshChanges)`, which queues runs per mesh (`m_changes`). `syncGpuMeshes` writes each
  merged point run into the position, normal and mask buffers and each triangle run
  into the index buffer (`glBufferSubData`). `upload(..., headroom)` allocates 1.25 ×
  the mesh plus 4,096 points / 8,192 triangles (`GpuMesh::pointRoom`/`triangleRoom`),
  so added detail is a write too; outgrowing the room uploads everything again, with
  more room.
- Qt 6.4 (Linux distributions) has a heap overflow in `QFormLayout` height-for-width
  with hidden rows (QTBUG-109237). The Properties panel keeps its wrapping "Made
  from" label outside the form, and `showRow` hides rows by their widgets before Qt
  6.5.

## Custom shape library (slice 45)

- `ShapeLibrary.h/.cpp` (`forge::library`): a custom shape is an ordinary `.f3d` file
  in the library folder, named by `fileNameFor` (characters Windows can't hold become
  '-', at most 80 bytes). `save` copies each selected shape's tree (`captureTree`) into
  a scratch `Document`, moves the roots so the group's box is centred on the plate's
  middle and resting on it, renames a single root after the shape, seeds the scratch
  document with any results already worked out (`cachedSolid` → `seedSolid`, valid
  because a result doesn't include the root's placement), and `file::save`s it, saved
  results included.
- `add` loads the file into a scratch document, `useSavedResults`, then for each root
  `cloneTree`s it into the model (new ids) shifted to where `suggestPlacement` puts a
  new shape, restores the saved names (a single root takes the file's name), adds it
  in one macro ("Add <name>", `abortMacro` on failure), and seeds each new root with
  the scratch document's result.
- App: `MainWindow::customShapesFolder()` (preference `library/folder`, else
  Documents/Forge3D/Custom Shapes). The Custom submenu of Add Shape is filled on
  `aboutToShow`. `CustomShapesDialog` lists, adds, renames (`library::rename`),
  deletes (`QFile::moveToTrash`) and changes the folder.

## Quick Socket (slice 46)

- `BooleanOp::Socket` (mesh engine), with `SocketFinish` (kind, size, depth, fit,
  printer, snap / grip, peg shape, turn, relief cuts). Like Emboss, the spot is the
  step's `planePoint` / `planeNormal` (facing out), in the coordinates of whatever holds
  the part. File format 26 (`"operation": "socket"`, a `"socket"` object).
- `Socket.h/.cpp`: `socket::cutters` builds closed meshes in a frame at the spot (z into
  the part; x across, as Emboss lays words, turned by `turn`): a ball (UV sphere, its
  corners pushed out so the facets' middles are on the true radius), a prism for pegs
  (polygon corners at `half / cos(π/n)`, so the flats are where asked) or, for round
  shapes, enough sides to stay within about 5 µm. Each starts `max(0.5, size/2)` outside
  the surface. A snap-in ball's middle is at `sqrt(R² − (opening/2)²)` below the opening.
  Relief cuts are slots from the axis out past the hollow. `Document::combineMeshes`
  subtracts them all from the part.
- `socketSpec` / `withSocket` / `socketSpot`; `surfaceUnder` picks on the part of a
  Socket step (as it does for Emboss), so a socket can be moved. `socket::matchingPart`
  gives a Sphere, Cylinder, Box, or a Pad of a hexagon / octagon sketch.
- `Scale`: the spot moves with the shape; scaled evenly, size and depth scale, the fit
  doesn't.
- App: `SocketPanel`, `MainWindow::SocketMode`. The preview is `previewMesh` of the
  step, or, for parts over 300,000 triangles, the cutters drawn as a red ghost. The last
  settings are kept in the preferences (`socket/...`).

## Join Parts (slice 47)

- `Join.h/.cpp` (`forge::join`): a `Feature` is a point and an outward direction in
  plate coordinates, of a kind (Flat, Peg, Hole, Ball, BallSocket, Socket, Spot), with
  a size and a suggested gap. `featureAt` casts the click against the shape's display
  mesh (`emboss::surfaceAt`), then: a Quick Socket step in its history whose hollow
  holds the hit (point: the ball seat's middle, or the peg socket's bottom; gap: its
  fit); for shapes not using the mesh engine, `measure::Shape` gives the face's kind:
  Flat (middle by area), Cylinder (ends from the face's triangles along the axis; the
  open end is the one further from the shape's middle; peg or hole from the hit
  normal against the axis), Sphere (ball or cup); for meshes, `ballAround` fits a
  sphere (least squares) to the points round the hit over growing reaches and keeps
  the largest that stays round (RMS < 2% of r, and the hit on it); then a round side:
  `patchAround` grows the smooth patch from the triangle under the hit (corners joined
  by place; edges bending under 30°), and `roundAround` takes the axis as the
  eigenvector of the area-weighted normals' covariance with the smallest eigenvalue
  (normals all across it, spread round it), fits a circle to the corners across it, and
  gives its ends along the axis; growing pieces of the patch round the click are tried
  and the largest still round is kept; else the spot.
- `mating`: the least turn taking the moving direction (flipped if asked) onto minus
  the fixed one, a spin about the fixed direction by `turn`, then the shift putting the
  moving point on the fixed point plus `along` along it and `acrossX/Y` in the Emboss
  frame. `joinedPlacement` applies it to the shape's world transform
  (`placementFromTransform`).
- `join::apply`: one macro, a `SetPlacementCommand`, then (kept together) `groupBodies`
  with the fixed shape, or, when the fixed shape is a Group, a `SeparateCommand` of it
  and a new group of its members plus the moving part, renamed as it was.
- Quick Socket's Center buttons use `centeredSpot` (Emboss's `centeredOnFace`), which
  picks on a Socket step's part as it does for Emboss.
- App: `JoinPanel`, `MainWindow::JoinMode` (`pick` finds the shape clicked; the moving
  shape's display item takes the new placement as a preview). Apply is one macro
  ("Join A to B") with a `SetPlacementCommand`.

## Split by Mask (slice 48)

- `BooleanOp::MaskSplit` (mesh engine), `MaskSplitFinish`: the dabs (centre, radius,
  erase) in the step's coordinates, in order, and which piece the step keeps. File
  format 27 (`"operation": "maskSplit"`, `"mask": {"piece", "dabs": [[x, y, z, r, erase]]}`).
- `MaskSplit.h/.cpp`: `mask::Field` files the dabs in a grid of cells as big as the
  largest; a point is masked if the last dab covering it painted. `mask::split`:
  `refineNearEdge` splits long edges whose middle is near a dab's sphere (1-, 2- and
  3-edge splits keep the surface joined) down to a quarter of the smallest radius; each
  edge crossing the mask's edge is cut where 32 halvings from its masked end find the
  edge; crossing triangles are split three ways. The masked piece's open edges are
  chained into loops; each loop gets rings drawn in to its middle, then 300 passes of
  umbrella smoothing (a membrane). The masked piece takes the caps, the rest takes them
  reversed: the pieces share the cap exactly.
- `splitByMask` mirrors `splitBody` (a copy of the shape for the second piece, both
  worked out first). `withMask` changes a step and, if it's still on the plate with the
  same dabs, its other piece (the dabs copied in step coordinates, since the pieces may
  have moved apart). `Scale` moves the dabs and grows them when scaled evenly.
- App: `MaskPanel`, `MainWindow::MaskMode`. The part's surface is shown (refined to
  about 60,000 triangles if coarser) with the paint as its shade (the sculpting mask's
  look); each dab sets the shade of the points it covers and sends just those runs to
  the graphics card (`Viewport::meshChanged`). Mouse jumps are filled with dabs a
  quarter of the brush apart.

## Reference images (slice 49)

- `Reference.h/.cpp`: `ReferenceImage` (direction, the picture file's bytes as read,
  shared; its size in pixels; mm per pixel; its middle along the view's right and up;
  turn; how far behind the plate's middle; opacity; shown; active). `reference::frameOf`
  gives each direction's right, up and towards-the-eye (the view cube's faces);
  `corners`, `pointAt`, `hit` (from its own side only), `pick` (active and shown only:
  inactive pictures are never in the way), `calibrated` (sized from two pixels and a
  length, the first point kept where it was), `modelLength` (a real length shown at
  1:N), `placed` (a new picture's size and spot), `validate` (one per direction).
- The pictures are not bodies: `Document::references()` / `setReferences()` (whole set
  at once; their own listener), changed through `SetReferencesCommand` (undoable;
  slider and drag steps merge while the label is the same). So nothing that works on
  bodies (export, Send to Slicer, printing splits, the custom shape library) ever sees
  them.
- File format 28: `"references"` in document.json, and each picture file stored as it
  was in `references/<view>.<kind>`. `file::load` still returns just the bodies;
  `file::loadReferences` reads the pictures (a broken one is left out, never a reason
  to refuse the file).
- App: `ReferencePanel` (a tool panel: View → Pictures → Reference Images), and
  `MainWindow::ReferenceMode` (sizing clicks, dragging). `Viewport::setPictures` draws
  them after the solid shapes, see-through, without writing depth, each only from its
  own side and fading out as it turns edge-on; textures are kept per picture file (at
  most 4096 pixels a side) and the chosen picture is outlined. Pictures are read with
  `Viewport::readPicture` (camera rotation applied) both for their size and for drawing.
  A click outside the panel that hits no shape but an active picture opens the panel on
  it; dragging it moves it. **Show Pictures** hides them all (the view only).

## Quick Hinge (slice 50)

- `BooleanOp::Hinge` (mesh engine), `HingeFinish`: the kind (single, double, swivel),
  the half this step makes (`Fork`: the slot and pin hole, or the swivel's bore with its
  groove; `Tab`: the part's end made into the tab, or the peg put on it), sizes, the pin
  (printed, 1.75 filament, rod), fit, stiff, swing, turn, reach, gap (double), lip
  (swivel) and `link`, a number both halves of one hinge share. The spot is the step's
  planePoint / planeNormal, on the fork's face, facing out of the fork. File format 29
  (`"operation": "hinge"`, `"hinge": {...}`).
- `Hinge.h/.cpp`: a frame at the spot (x along the pin, from `emboss::upFor` and the
  turn; z into the fork). `forkCutters`: the slot down to the pin, a round room for the
  tab's end, deeper slots on the side(s) it bends to, the pin hole through the arms
  (pin + fit). `tabHalf`: clears the other part inside the reach and `fit` off the
  fork's face, adds the tab (or the peg), cuts its pin hole (pin + fit/2 when stiff,
  + 2 fit when free; the peg's slit). `pieces`: the tab, link or peg and printed pins,
  in place, as closed meshes; `pieceBody` makes each an imported-style mesh shape.
  `otherHalf`: the partner's spot and settings (a double hinge's second fork faces back,
  `gap` out, its turn worked out so the pins line up, forward and back swapped).
  `hingePartner` finds the other half by its link number.
- Scaling evenly grows the joint, and a printed pin, but not a filament pin or rod, nor
  the fit.
- App: `HingePanel`, `MainWindow::HingeMode` (like Quick Socket: a spot on the part,
  centring on the face, preview or the red cut on big shapes, a blue ghost of the other
  half). Apply is one undo step: the fork step, the other part's step (if picked), and
  the pieces. A printed pin is cut to the part's width there (rays from both sides along
  the pin). Changing either half (double-click) changes its partner too.


## Scale Calculator and mask colours (0.59.1)

- `ScaleCalculator` (Tools menu, a modeless dialog, made when first opened):
  `reference::modelLength` and `reference::fromMm` (units now include yards);
  settings kept under `scaleCalculator/*`.
- Painting a mask: `DisplayItem::maskColours` (with `unmasked` / `masked`) makes the
  mesh shader draw the shade as a mix of two plain colours with nearly flat light
  (`uMaskOn`). Used for Split by Mask's part and for the sculpted part while the Mask
  brush is chosen (`SculptPanel::brushChanged` refreshes the scene). The colours live
  in `MaskPanel` (preferences `mask/unmaskedColour`, `mask/maskedColour`).

## Pose Test (slice 51)

- `Pose.h/.cpp`: `findJoints` reads the Hinge (fork) and Socket steps: pivot (the pin's
  middle, the ball's middle, half way down a bore), axis, kind (hinge, swivel, ball,
  fixed), the holder (`history::objectOf` the step) and the other part (the tab's, by
  the hinge's link number, else the biggest part on the plate whose box holds the
  pivot). `build` makes the rig: a breadth-first tree from the held part (the biggest
  jointed one unless chosen), other groups from their biggest part, and parts with a
  pivot inside them (pins) riding with the joint's parent side. `transforms` composes
  each joint's turn (about the build-pose pivot) down the tree; `turn` /
  `ballAngles` (bend, sideways, twist: R(across) R(axis × across) R(axis)).
  `collisions` intersects pairs that don't move together (bounding boxes first);
  `range` steps a hinge 5° then 1° each way until parts it moves hit others.
- The pose never touches the bodies: `MainWindow::PoseMode` keeps the angles and the
  moved transforms, multiplies them into the display items' models, and picks with
  `pick(..., posed)`. Hit checks and Find Range run on `QThreadPool`, on copies of the
  parts' solids, and their answers are dropped if the pose has changed since
  (`generation`).
- `SavedPose` (name, held part, angles by step id) in `Document::poses()`, changed with
  `SetPosesCommand`; file format 30 (`"poses"`), read with `file::loadPoses`.

## Tolerance test print (slice 52)

- `Tolerance.h/.cpp`: `steps` (the fits, to 0.01 mm, at most 12), `label` (".15"),
  `digits` (raised seven-segment numbers, no font needed), and `pieces`: a bar per kind
  with `socket::cutters` at each fit (a round peg; a snap ball; a through hole for a
  pin), labels merged on, and the pieces to try (pegs, a ball on a stalk, pins), all as
  closed meshes standing on the plate. The app adds them as mesh shapes
  (`hinge::pieceBody`) in one undo step, beside what's on the plate.
- `app/Fits.h/.cpp`: the measured fits per printer and kind (preferences
  `fit/<resin|fdm>/<peg|ball|pin>`); `fits::starting` gives the measured fit or the
  usual 0.1 / 0.2. SocketPanel and HingePanel use it when the printer (or the socket's
  kind) changes; recording also updates `socket/fit`, `hinge/fit` and
  `split/clearance`. `ToleranceDialog` (Tools menu) makes the test and records.

## Clicks and ridges (slice 53)

- `HingeFinish::clicks` / `clickDepth`: `wedge` makes a triangular ridge or groove;
  `clicksOf` gives where they run (round the pin from the tab hole to the end's edge,
  or along a swivel's peg between the face and the lip) and their width (no more than
  1.2 × the depth, and 0.225 of the spacing). `tabRidges` (the tab, the tab half, each
  end of a link) and `armGrooves` (fork cutters: the groove is the ridge grown by
  1.5 × the fit and set the fit back); `swivelClicks` for pegs and bores. At every
  click the ridges clear the grooves; between, they overlap the arms by about the depth
  less the fit.
- `SocketFinish::ridges` / `ridgeHeight`: the ball cutter has slices of its outer shell
  (a slot through the axis minus the ball shrunk by the height) taken out of it, so
  ribs stay in the cup.
- `pose::Joint::clicks` and `nearestClick`: Pose Test snaps such joints to their
  clicks, and `range` steps by clicks. File format 31.

## Magnet pockets (slice 54)

- `SocketKind::Magnet` (size: the magnet's diameter, depth: its thickness) and
  `SocketFinish::cover` (a skin over it; 0 open). `socket::cutters` cuts a round prism
  from outside the surface (or from `cover` down) to `cover + depth + fit`; no relief
  cuts. `matchingPart` gives a stand-in cylinder; `facingSpot` finds the matching
  pocket's spot on another part: its surface straight out from the spot, within 3 mm,
  facing back. The app cuts both pockets in one macro.
- Pose Test: a magnet is a `Fixed` joint; the held part is found just outside the face.
- File format 32 (`"kind": "magnet"`, `"cover"`).

## Print Check (slice 55)

- `printcheck::faceThickness`: per triangle, a ray from its middle straight in (against
  its outward normal, following the vertex normals if the winding disagrees) to the
  nearest other triangle, up to `kMaxWall` (5 mm), so changing the setting doesn't
  measure again. The triangles go in a uniform grid (about two triangles across a cell,
  at most about four million cells) walked cell by cell (Amanatides–Woo); big meshes
  are split over `std::thread`s.
- `printcheck::classify`: thin (thickness under the setting) and overhang (the normal,
  placed on the plate, below −sin(angle); faces whose corners all lie within 0.05 mm of
  the part's bottom rest on the plate). Builds the shown mesh with each triangle's own
  corners and `Mesh::shade` 0 / 0.5 / 1, and the areas.
- App: `PrintCheckMode` keeps each part's measured mesh and thickness (measured on
  `QThreadPool`, dropped if the part changed since); the settings only re-classify.
  `DisplayItem::checkColours` draws the shade as three flat colours (`uMaskOn` 2 and
  `uOverhang` in the mesh shader).

## Rescale Design (slice 56)

- `Body::hardware` marks a joint part (file key `"hardware"`, left out of the result
  fingerprint; `Document::setHardware`, carried by `EditBodyCommand`). Set by Quick
  Socket's matching part, Quick Hinge's pieces and PrintSplit's `coneBody` and loose
  pins.
- `ScaleOptions{keepJoints, keepWalls}` threads through `scaledTree` (even factors
  only). With `keepJoints` a hardware body is moved rigidly by its own origin
  (`movedRigidly`); Socket, Hinge and Thread steps keep their sizes while their plane
  points move. `keepWalls` keeps `ShellFinish::thickness`.
- `rescaledDesign(doc, ids, factor, anchor, options)`: each top-level body by
  `scaledTree` with the anchor taken into its own frame (a top-level hardware body
  moved whole). `jointCount` counts hardware bodies and joint steps. The app anchors
  at the middle of the footprint at its bottom and applies one `EditBodiesCommand`.

## Figure Guide (slice 57)

- `Mannequin` (Mannequin.h): height, heads, place, turn, shown, and two angles for each
  of ten joints (`MJoint`). `mannequin::landmarks` and `mesh` work out the skeleton
  from the height in heads (head H = height / heads; the rest R = height − H; joint
  heights as shares of R) and pose it by forward kinematics: root → waist → neck and
  shoulders → elbows; root → hips → knees. The figure faces −Y, its left at +X. A
  second pass lifts or lowers the root so the lowest foot (an ellipsoid turned with the
  shin) rests on the plate. Drawn as ellipsoids and rounded rods.
- `Document::mannequin` / `setMannequin` (validated) / `addMannequinListener`;
  `SetMannequinCommand` merges runs with the same label (`kMergeMannequin`).
- File format 33: `"mannequin"` in document.json; `file::loadMannequin` /
  `mannequinFromJson` (a broken one is left out).
- App: `MannequinPanel` (a tool panel like Reference Images); the mesh is rebuilt on
  each change and drawn as a see-through helper item that Fit still counts
  (`DisplayItem::fitted`).

## Hollow (slice 58)

- `BooleanOp::Hollow` with `HollowFinish` (thickness, hole size, `DrainHole`s: point and
  outward normal in step coordinates; file format 34, `"hollow"`). A mesh-engine step.
- `hollow::hollowed`: voxels of `cellFor` (a third of the wall, grown to keep under 16
  million): inside found by a ray up each column (triangles binned by column), then an
  exact squared Euclidean distance transform (Felzenszwalb–Huttenlocher, three 1D passes,
  over the cores) to the nearest outside cell; distance to the surface ≈ that − half a
  cell. The cavity is `meshkernel::levelSet` (Manifold `LevelSet`) of the trilinear field
  at the wall thickness, sampled coarser than the grid (it's never seen), subtracted
  from the part. Not deep enough anywhere: the part is returned as it was.
- `holeCutter`: a 24-sided rod from 1 mm outside the spot to 1.5 × the wall + 1 mm in.
  `lowSpots`: triangle middles, lowest first (then most downward), at least four hole
  sizes apart. `plug`: a cylinder, hole radius less the fit, wall-thick.
- `hollowSpec` / `hollowInPlate` / `withHollow` move the hole spots between plate and
  step coordinates (like Socket). Scale moves the spots; `keepWalls` keeps the thickness,
  `keepJoints` the hole size.
- App: `HollowPanel`, `HollowMode` (holes in plate coordinates, red cutters shown); plugs
  added with the step in one macro, as hardware bodies.

## Surface stamps (slice 59)

- `BooleanOp::Stamp` with `StampFinish` (kind, size, height, sunk, spin, spacing, and
  `StampSpot`s: point, normal and `along`, in step coordinates; file format 35,
  `"stamp"`). A mesh-engine step. `StampKind::Custom` takes a second operand: the shape
  used as the stamp (a custom shape from the library, added and taken into the step).
- `stamp::shape`: built-in stamps made in mm on the origin facing +Z (revolved domes,
  prisms, stadium slots; the screw's slot subtracted), each with a stem
  max(0.4 × size, 0.5 mm) the other way; sunk is the same mirrored in Z. A custom shape
  is fitted by its bounds to size × size × height and sunk 15 % into the surface.
- `stamp::frame`: origin on the point, Z the normal, X `along` made square to it, then
  turned by spin. `apply`: raised merges the part with every placed stamp (one batch);
  sunk subtracts them. `rowPoints` spreads points evenly so the last lands on the end.
- App: `StampPanel`, `StampMode` (spots in plate coordinates, `groups` for Remove Last).
  A press records the row start, a drag rebuilds the row (each point dropped onto the
  surface along the averaged normal), a release keeps it; a click adds one.
- Scale moves the spots and, evenly scaled, grows the size, height and spacing.

## Panel lines (slice 60)

- `BooleanOp::PanelLines` with `PanelLineFinish` (profile, width, depth, and
  `PanelLine`s of `PanelPoint`s: point and normal, close together, in step coordinates;
  file format 36, each point stored as six numbers). A mesh-engine step.
- The app lays each line between clicks: points every `panel::pointSpacing(width)`
  along the straight run, each dropped onto the surface along the two clicks' averaged
  normal (`surfaceUnder`), so the step itself needs no ray casting.
- `panel::cutters`: one convex prism per pair of neighbouring points, the section
  (V: a pentagon with a point at the depth; square; round: a half-ellipse) set upright
  to the surface at each end and square to the piece, each end reaching on
  min(width/2, half the piece) so bends have no gaps. `apply` subtracts them all in one
  batch; a closed line has a piece back to its start.
- App: `PanelLinePanel`, `PanelLineMode` (finished lines, the clicked points of the line
  being drawn and the surface pieces between them, re-laid when the width changes).

## Straight lines (0.69.1)

- `MainWindow::straightPlane`: Shift gives the level plane through the line's last
  point (normal +Z); Ctrl the upright one across the view (normal up × the view's level
  direction). `onSurfaceInPlane` moves a point into the plane, then back onto the
  surface along its normal's part lying in the plane, so the point stays in it.
- Panel Lines keep each piece's plane (`PanelLineMode::planes`) to lay it again when the
  width changes; Stamp rows use the plane through the row's start.

## Split for painting (slice 61)

- `MaskSplitFinish` gains `keys` and `keyFit` (file format 37: `"keyFit"` in the step's
  `"mask"`, present only when keyed).
- `mask::split` also gives a `KeySpot` per cap: the cap's middle point after smoothing,
  the cap's area-weighted normal (out of the masked piece into the rest), a direction
  across it (towards the loop's first point) and the room (nearest loop point).
- `mask::keyed`: an isosceles triangular prism per spot big enough (`keySize`: a third
  of the room, at most 3 mm, none under 0.5 mm; `keyLength` 1.5 × that, 1–4 mm). The
  masked piece merges pegs from −length to +length; the rest subtracts them grown by
  2 × fit (in the size) from −0.5 to length + fit. Both pieces work the spots out from
  the same mask, so they match.
- `maskSplitSpec` / `splitByMask` take a key fit (negative: none); `withMask` can change
  the keys on both pieces.

## Kit layout (slice 62)

- `kit::plan`: parts sorted biggest first (depth, then width), packed into shelf rows
  about √(total area) × 1.1 wide; rows stacked with a bar under each and one over the
  last, side bars, and `gap` (grown to fit the number tags) all round. Each part's gate
  runs from the nearer bar (mid-height) to the part's point scoring lowest on distance
  to that bar plus half its height off the bar (a few thousand of its points sampled),
  0.4 mm into it. Tags (1 mm plates) with `tolerance::digits` numbers sit beside each
  gate in the gap. Everything is merged into one closed mesh.
- `kit::make`: one macro: an `EditBodiesCommand` moving (and renaming) the parts, the
  frame added as a mesh body (`hinge::pieceBody`), and optionally a Merge of the frame
  and parts.
- `EditBodyCommand` / `EditBodiesCommand` now apply the name as well.

## Kit name tag (0.71.1)

- `kit::Settings::name` / `includeName`: the words are made with `emboss::letters`
  (raised 0.6 mm) on a big flat plate first to measure them, shrunk to fit the frame's
  width if need be (2 mm letters at least), then laid on a 1.2 mm tag under the bottom
  bar, overlapping it 0.2 mm, and merged with the frame.

## Parts list (slice 63)

- `parts::rows`: per top-level shape (and, if asked, the operands of a Merge or Group
  under it at depth 1) its world-box size, `meshkernel::volume` of its solid, weight =
  volume (cm³) × density × fill × (1 + waste), cost = weight × price per kg. `total`
  sums the depth-0 rows. `csv` (quoted where needed) and `html` (escaped) render it.
- App: `PartsListDialog` (material presets from `parts::materials`, recomputed on every
  change through a callback to the document); copy as tab-separated cells, CSV with a
  byte-order mark, PDF through `QTextDocument::print` on a `QPdfWriter` (Qt GUI only, no
  print-support module).

## Tidy-up towards 1.0 (slice 64)

- Help: `help/figure-tools.html` (in `help.qrc`), shown by `MainWindow::showFigureTools`
  in a `QTextBrowser` dialog like the Quick Start, from Help → Figure, Toy and Kit Tools.
- `tests/test_heavy.cpp`: hidden `[.heavy]` timings on a 1.3-million-triangle bumpy ball
  (Print Check measuring and colouring, Hollow, a row of Stamps, Panel Lines, the shown
  mesh). Not run by CTest; run `forge_tests "[.heavy]" -s` to see them.
- `docs/release-notes/v0.73.0.md`: the notes the publish step needs for v0.73.0.

## Pose Figure (slice 65)

- Core `Rig.h` / `Rig.cpp` (namespace `rig`): a 19-point skeleton (`rig::Point`) and 14 joints (`rig::Joint`: the
  figure guide's 10 in its order, then wrists and ankles), 15 bones. `BooleanOp::Posed` with `PosedFinish` (points
  in the step's coordinates, absolute angles in the figure guide's convention, blend, feetFlat, onGround); a mesh
  engine step, file format 38 (operation "posed").
- Angles are absolute (0 = standing straight), so the guide's presets work on any figure. Each joint's rest turn is
  measured from the points; `restAngles` gives the angles it was made in, and a per-joint correction
  (`local(rest)^-1 * restTurn`) carries what the angles can't say (a twist, a lean), so the made angles move
  nothing exactly.
- `rig::fit`: torso from 7.5-head proportions of the mesh's height, centred on the waist's surface; arms out if
  something reaches past the shoulder above mid-thigh (else down); feet from the lowest surface each side; elbows,
  wrists, knees and shoulders centred across the limb.
- `rig::Skin`: weights per vertex to the nearest bones (distance to each bone's segments; smoothstep falloff over
  `blend` from the nearest, top four kept), worked out once (multithreaded); `posed()` blends dual quaternions.
  About 0.1 s for weights and 0.02 s per pose on 650 thousand points.
- App: `PoseFigurePanel` (stages Fit / Pose), `MainWindow::PoseFigureMode` (rest mesh in step coordinates, the
  skin, the posed preview, the skeleton drawn ghosted on top). Dragging a ball: fitting moves it in the view plane
  (mirrored about the waist's X); posing searches the joint above it for the angles that point its bone at the
  mouse (`rig::moves`, a halving pattern search within the joint's range).

## Paint (slice 66)

- Core `Paint.h` / `Paint.cpp` (namespace `paint`): `BooleanOp::Paint` with `PaintFinish` (dabs in order:
  centre in the step's coordinates, radius, colour 0xRRGGBB, erase). A mesh engine step whose shape is its
  part's, divided finer (`paintSurface`: `sculpt::refined` to about 60 thousand triangles) when it has fewer.
  File format 39 (operation "paint": a "colours" list of "#RRGGBB" and dabs `[x, y, z, r, colour number]`).
- Colours are spatial, like Split by Mask's dabs: `paint::Field` files dabs in a grid (dabs much bigger than most,
  such as Fill All, kept apart and checked everywhere); a triangle takes the last dab over its middle
  (`triangleColours`). `objectPaint` gathers every switched-on Paint step of an object's history into its own
  coordinates, so later steps that move the shape rigidly keep the paint on it (steps that reshape it, such as a
  pose after painting, don't carry it).
- Display: `Mesh::paint` (four floats per point: colour and how much) as vertex attribute 3; the mesh shader mixes
  it over the shape's colour. `MainWindow::paintedMesh` caches each painted object's coloured copy by mesh pointer
  and dabs. The Paint tool shows the fine surface with each triangle's own corners and updates only the triangles
  under each dab (their middles filed in a grid).
- 3MF: `ExportMesh::triangleColours`; base materials hold the shapes' colours then the painted ones; painted
  triangles get `pid`/`p1`, plus `slic3rpe:mmu_segmentation` and `paint_color` (filament n: "4", "8", then
  "0C".."DC") when there are 16 colours or fewer.
- Split by Mask: `paint::maskFor` turns one colour's dabs into mask dabs (other colours' dabs rub out).

## Joint standards (slice 67)

- Core `JointStandard.h` / `.cpp` (namespace `standards`): `Standard` (name, notes, joints) and `Joint` (name, a
  `SocketFinish` or a `HingeFinish`). `applied` puts a joint's sizes on a socket or hinge being made, keeping its
  fit, printer, turn (and a hinge's half and link); `jointFrom` keeps the sizes from one. `toleranceSizes` sizes
  the tolerance test from the first ball, round peg and printed hinge pin. JSON ("forge3d-joint-standards",
  version 1) for the library and for sharing; not part of the .f3d file.
- App: `Standards.h` (`standardsfile`: Documents/Forge3D/Joint Standards.json and the one in use, a preference;
  `StandardRow`, the row at the top of SocketPanel and HingePanel), `JointStandardsDialog` (Tools menu),
  `MainWindow::refreshStandardRows` / `saveToStandard`.

## Cut into Parts (slice 68)

- Core `LimbCut.h` / `.cpp` (namespace `limbcut`): `BooleanOp::LimbCut` with `LimbCutFinish` (the joint, the cut's
  point and normal, the limb's reach as rods, the gap, which piece). A mesh engine step: the reach (merged rounded
  rods, `rod`) trimmed at the cut plane, intersected with the part (the limb) or subtracted from it (the rest).
  File format 40 (operation "limbCut").
- `plan`: the cut through the joint's ball, square to the limb (halfway between the two bones at elbows, knees,
  wrists and ankles); the reach rods along the bones below, each as fat as nearly all (97 %) of the surface nearest
  that bone (`rig::nearestBones`), so a stray point (the crotch nearest a thigh) doesn't swell it; the limb's
  thickness at the cut, the median distance of the limb's surface round it.
- `cutIntoParts`: from the middle out, each cut makes the rest (on the piece) and the limb (on a copy, its solid
  seeded so it isn't worked out again); each bone's piece is tracked, so later cuts find their limb, and pieces are
  named by `rig::pieceName`. Then the joints on the finished pieces: a ball joint is a Socket step in the limb and a
  Merge of a ball on a neck (`ballOnNeck`, an imported-style body) onto the other piece; a hinge is a fork step on
  the piece nearer the middle and a tab step (`hinge::otherHalf`) on the limb, sharing a link number, the pin square
  to the limb and the front, the slot reaching just through the limb, and the pins (`hinge::pieces`) as joint parts.
- Rig: bones exposed (`rig::boneOf`, `parentBone`, `boneAndBelow`, `boneTitle`, `pieceName`, `boneSegments`,
  `nearestBones`). (Fixed: `boneSegments` looped over a part of a temporary.)
- App: `CutPartsPanel` (skeleton, Usual Figure, a joint at a time, what goes in, printer) and
  `MainWindow::CutPartsMode` (the figure see-through, the skeleton and red discs ghosted; balls dragged as in Pose
  Figure; the cuts planned again when a ball is let go).
