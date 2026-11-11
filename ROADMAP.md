# Forge3D roadmap

The full product spec lives in the project doc "3D Design Software — Product Spec &
Architecture Plan". This file tracks delivery.

## Phase 1: Build (replaces 3D Builder)

Gate to Phase 2: a new user prints a customised part within 10 minutes.

| Slice | Content | Status |
| --- | --- | --- |
| 1 | Project skeleton, Windows CI, OpenGL viewport, box with orbit / pan / zoom | Done (0.1.0) |
| 2 | Document model, command stack, undo/redo, unit tests | Done (0.2.0) |
| 3a | Selection, Objects and Properties panels, Save / Open (`.f3d`), 3-axis rotation | Done (0.3.1) |
| 3b | Gizmo: drag to move, rotate and resize; snapping | Done (0.4.0) |
| 3b+ | Uniform scale; elliptical cylinders | **Delivered for testing** |
| 3c | Merge / Subtract / Intersect; primitive library replaces the test buttons | Next |
| 4 | Merge, Intersect, Split by plane | Planned |
| 5 | STL / 3MF import with auto-repair; export; "Send to slicer" | Planned |
| 6 | Feature tree panel with rollback bar | Planned |
| 7 | 2D sketcher with constraints | Planned |
| 8 | Pad, Pocket, Revolve, Fillet, Chamfer, Shell, Hole | Planned |
| — | Ribbon UI with Build / Design / Sculpt / Print tabs (grows across slices) | Planned |

Separate track: sculpt-engine prototype (one Draw brush on a 1M-triangle mesh at 60 fps).

## Phase 2: Sculpt · Phase 3: Hybrid · Phase 4: Pro

See the spec doc. Slices will be listed here when Phase 1 nears its gate.
