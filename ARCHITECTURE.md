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
- Opening replaces all bodies with `Document::replaceAll()` (checks everything first) and
  clears the undo history.

Linux development builds need miniz from outside the system packages: install it to a
prefix and configure with `FORGE3D_EXTRA_PREFIX=<prefix> cmake --preset linux-dev`.

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
