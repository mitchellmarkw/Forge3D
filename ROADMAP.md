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
| 3b+ | Uniform scale; elliptical cylinders | Done (0.4.2) |
| 3c | Multi-select; Merge / Subtract / Intersect as editable results; Separate | Done (0.5.0) |
| 3c+ | Results turn about their visible centre; Align Shapes | Done (0.5.1) |
| 3d | Primitive library (sphere, cone, torus, tube, wedge) replaces the test buttons | Done (0.6.0) |
| 3d+ | Oval sphere / cone / torus / tube; pyramid | Done (0.6.1) |
| 4a | Split by plane (keep one or both halves; move the cut later) | Done (0.7.0) |
| 4a+ | Drag the Split plane in the view; handles right after a split | Done (0.7.1) |
| 4b | Handles on parts inside combined shapes; resize handles for combined shapes | Done (0.8.0) |
| 4c | Mirror, Duplicate (copy / paste), Group, move / turn several shapes at once | Done (0.9.0) |
| 5a | Export STL / 3MF / OBJ; "Send to Slicer" | Done (0.10.0) |
| 5b | STL / 3MF / OBJ import with auto-repair | Done (0.11.0) |
| 5c | Mesh engine: fast Merge / Subtract / Intersect / Split / Mirror / Group on big imported meshes | Done (0.12.0) |
| 5d | Text shape; Emboss words that wrap round the surface; stacked letters | Done (0.13.1) |
| 5e | Rounding / fillet / chamfer / bevel edges, picked in the view; coves | Done (0.14.1) |
| 6 | History (feature tree) with rollback bar, switching steps off, steps added at the bar; Through all | Done (0.15.1) |
| 7 | 2D sketcher with dimensions and rules, on the plate or a flat face; Pad and Pocket | Done (0.16.0) |
| 8 | Revolve / Groove, Hole (plain, counterbored, countersunk, metric sizes), Shell | Done (0.17.0) |
| 9 | Sketch on middle planes (grooves round parts); ribbon with Build / Design / Sculpt / Print tabs | Done (0.18.0) |
| 10 | Follow: grooves and ridges along a path drawn on the surface (flat or curved); round / square / V / drawn cross-sections | Done (0.19.1) |
| 11 | Patterns: linked rows, grids and circles of a shape or its last feature | Done (0.20.2) |
| 12 | Emboss a picture: logos traced to an outline, photos as reliefs; flat and curved surfaces | Done (0.21.0) |
| 13 | Phase 1 polish: Lay on Face, Sit on Plate in the ribbon, a ribbon that fits narrow windows, a first step on an empty plate, double-click fix | Done (0.22.0) |
| 14 | Icons: a picture on every ribbon button (name underneath) and in the menus; shape pictures in Add Shape | Done (0.23.0) |
| 15 | Quick start: an illustrated first-print guide (in the app and on GitHub) and a guided tour the first time Forge3D runs | **Delivered for testing** |

Separate track: sculpt-engine prototype (one Draw brush on a 1M-triangle mesh at 60 fps).

## Phase 2: Sculpt · Phase 3: Hybrid · Phase 4: Pro

See the spec doc. Slices will be listed here when Phase 1 nears its gate.
