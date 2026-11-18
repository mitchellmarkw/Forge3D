# Phase 1 gate review (2026-11-17, at 0.47.1)

Phase 1's goal is to replace Microsoft 3D Builder. Its gate: **a new user prints a
customised part within 10 minutes.**

## 3D Builder, feature by feature

| 3D Builder | Forge3D | |
| --- | --- | --- |
| Shapes library | Add Shape: box, cylinder, sphere, cone, torus, tube, wedge, pyramid, text (ovals too) | ✓ |
| Move / rotate / scale, lock proportions, settle on the plate | Gizmo, Properties, Uniform, Sit on Plate, Lay on Face | ✓ |
| Merge / Subtract / Intersect | The same, and editable afterwards | ✓ |
| Split | Split, with pegs, and Split to Fit | ✓ |
| Emboss text and images | Emboss words and pictures, flat or wrapping round | ✓ |
| Mirror, duplicate, group | The same, plus Patterns | ✓ |
| Simplify / Reduce | Simplify step | ✓ |
| Repair on import | Auto-repair with a report | ✓ |
| Hollow | Shell | ✓ |
| Print (to a slicer) | Export, Send to Slicer, plate size and fit warning | ✓ |
| Open / save STL, OBJ, 3MF | The same | ✓ |
| Paint: colours on objects, kept in 3MF | Colour, per shape (0.48); painted regions not yet | ✓ |
| Smooth (the whole model, one click) | Smooth step, strength 1–10 (0.49) | ✓ |
| Extrude Down (stretch a model's underside down to the plate) | Extrude Down step (0.49) | ✓ |
| Open PLY and GLB (scans, downloads) | PLY, glTF and GLB too (0.50) | ✓ |
| Wireframe view | Solid / See-Through / Wireframe (0.49) | ✓ |
| Webcam capture, Bing 3D library | Not planned (services Forge3D doesn't have) | — |

Beyond 3D Builder, Phase 1 already includes FreeCAD-style history, sketches, holes,
threads, loft and sweep, plus sculpting (the start of Phase 2).

## The 10-minute test

The Quick Start (a key tag with a hole and a name, then Send to Slicer) takes seven
steps. Each one was checked while the guide's pictures were made. The guided tour runs
on first start. The real test needs **someone who has never seen Forge3D**: give them
the installer and the Quick Start, time them, and note where they hesitate. Nothing in
the code can stand in for that.

## Closing the gaps

| Slice | Content |
| --- | --- |
| 39 | Colours: choose a shape's colour (remembered in the file, kept by Merge and friends) and export them in 3MF for multi-colour printers |
| 40 | Smooth (a step for the whole shape) and Extrude Down; Solid / See-Through / Wireframe looks |
| 41 | Open PLY and GLB/glTF |

After that come the agreed next steps: angled smart cuts, laying out split pieces,
then the sculpt-engine prototype.
