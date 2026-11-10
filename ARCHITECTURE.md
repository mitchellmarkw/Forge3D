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

## Data flow today (slice 1)

`primitives::makeBox()` → exact `TopoDS_Shape` → `tessellate()` → `forge::Mesh`
(positions, normals, indices) → `Viewport::setMesh()` → GPU buffers → drawn each frame.

Tessellation gives each B-rep face its own vertices, so normals are smooth within a face
and sharp across edges.

## Conventions

- Namespaces: `forge` (core), `forge::primitives`, `forge::view`, `forge::app`.
- Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.
- Log with `qInfo()`, `qWarning()`, `qCritical()` in app code; they go to the log file.
- Each delivered slice gets a test sheet in `docs/test-sheets/` and a `CHANGELOG.md` entry.
