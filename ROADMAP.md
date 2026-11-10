# Forge3D roadmap

The full product spec lives in the project doc "3D Design Software — Product Spec &
Architecture Plan". This file tracks delivery.

## Phase 1: Build (replaces 3D Builder)

Gate to Phase 2: a new user prints a customised part within 10 minutes.

| Slice | Content | Status |
| --- | --- | --- |
| 1 | Project skeleton, Windows CI, OpenGL viewport, box with orbit / pan / zoom | **Delivered for testing** |
| 2 | Document model, command stack, undo/redo, unit tests | Next |
| 3 | Primitive library + gizmo + Subtract boolean, save and reopen (`.f3d`) | Planned |
| 4 | Merge, Intersect, Split by plane | Planned |
| 5 | STL / 3MF import with auto-repair; export; "Send to slicer" | Planned |
| 6 | Feature tree panel with rollback bar | Planned |
| 7 | 2D sketcher with constraints | Planned |
| 8 | Pad, Pocket, Revolve, Fillet, Chamfer, Shell, Hole | Planned |
| — | Ribbon UI with Build / Design / Sculpt / Print tabs (grows across slices) | Planned |

Separate track: sculpt-engine prototype (one Draw brush on a 1M-triangle mesh at 60 fps).

## Phase 2: Sculpt · Phase 3: Hybrid · Phase 4: Pro

See the spec doc. Slices will be listed here when Phase 1 nears its gate.
