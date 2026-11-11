# Changelog

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
