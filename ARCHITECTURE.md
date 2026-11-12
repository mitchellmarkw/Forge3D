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
