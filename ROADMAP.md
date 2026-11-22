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
| 15 | Quick start: an illustrated first-print guide (in the app and on GitHub) and a guided tour the first time Forge3D runs | Done (0.24.1) |
| 16 | First public release (0.25 Beta): installer, portable zip, GitHub releases from a version tag, app icon, Check for Updates | Done (0.25.0, released) |
| 17 | Sculpting prototype: Draw and Smooth brushes on a dense copy of a shape (Phase 2 begins) | Done (0.26.0) |
| 18 | More brushes (Inflate, Flatten, Pinch, Clay, Grab) and mirror symmetry (X / Y / Z) | Done (0.27.0) |
| 19 | Detail where you sculpt (triangles added under the brush) and faster picking | Done (0.28.0) |
| 20 | View cube: which side faces you; click a side, edge or corner to look from there | Done (0.29.0) |
| 21 | Sculpting kept in the history (a Sculpt step on top of the parametric steps) | Done (0.30.0, released) |
| 22 | Mask (paint, clear, invert), Crease and Snake hook brushes | Done (0.31.0) |
| 23 | Remesh (even triangles, in sculpting) and Simplify (a step for any shape) | Done (0.32.0, released) |
| 24 | Pattern brush: built-in patterns or a picture pressed in, repeating | Done (0.33.0) |
| 25 | Measure: faces, edges, corners and points; distances, angles, diameters, centres | Done (0.34.0) |
| 26 | Threads: metric screw threads on pegs and in holes, with a fit for printing | Done (0.35.0) |
| 27 | Loft and sweep: blend between outlines; sweep an outline along a drawn path or a spiral | Done (0.36.0, released) |
| 28 | Sketch tools: Trim, Round corner, Offset (held by Gap dimensions), Mirror | Done (0.37.0) |
| 29 | Taper (draft) on pads and pockets | Done (0.38.0) |
| 30 | Text in sketches: words whose letters are outlines | Done (0.39.0) |
| 31 | Section view: cut the view open to look inside | Done (0.40.0) |
| 31+ | Section view for only the selected shape | Done (0.40.1, released) |
| 32 | Open Recent; build plate size; the ribbon reorganised, with a View tab (Orbit / Pan / Zoom on the left button) | Done (0.41.0) |
| 33 | Printer height, and a warning colour for shapes that don't fit the plate | Done (0.42.0) |
| 34 | Backup copies while you work, offered back after a crash | Done (0.43.0) |
| 35 | Backup settings; clicks respect the section view; measurements shown and kept in the view | Done (0.44.0, released) |
| 36 | Sketch on the section view's cut | Done (0.45.0, released in 0.47.1) |
| 37 | Dark theme (View → Theme: Follow Windows, Light, Dark) | Done (0.46.0, released in 0.47.1) |
| 38 | Split for printing: manual and smart splits to fit the printer, with tapered alignment pegs and matching holes | Done (0.47.0, released in 0.47.1) |

| — | Code audit: damaged files, extreme sizes, safety net | Done (0.47.1, released) |
| 39 | Colours: a colour for each shape, kept in the file and exported in 3MF | Done (0.48.0) |
| 40 | Smooth (whole shape), Extrude Down; Solid / See-Through / Wireframe | Done (0.49.0) |
| 41 | Open PLY and GLB/glTF | Done (0.50.0) |
| 42 | Angled smart cuts; drag a cut before it's made | Done (0.51.0) |
| 43 | Lay out split pieces on the plate (or several plates) | Done (0.52.0) |
| 44 | Sculpt-engine prototype: Draw brush on a 1M-triangle mesh at 60 fps | Done (0.53.0) |
| 45 | Custom shape library: save shapes, add them again at their exact size | Done (0.54.0) |
| 46 | Quick Socket: ball and peg sockets cut in at a clicked spot, sized to fit once printed | Done (0.55.0) |
| — | Features tab: Hole, Thread, Quick Socket, Shell, Follow, Edges and Pattern move off the Design tab | Done (0.55.1) |
| 47 | Join Parts: a peg into its socket, a ball into its cup, a face against a face | Done (0.56.0) |
| 48 | Split by Mask: paint where to cut; both pieces closed with the same smooth cap | Done (0.57.0) |
| 49 | Reference images: a picture per side, sized from two points, opacity, active or not | Done (0.58.0) |
| 50 | Quick Hinge: single and double hinges and swivels, with pins, sized to fit once printed | Done (0.59.0) |
| — | Tools menu with a Scale Calculator; mask painted in plain, high-contrast colours you can choose | Done (0.59.1) |
| 51 | Pose test: turn the joints, see where parts hit, find each hinge's range, save poses | Done (0.60.0) |
| 52 | Tolerance test print: holes, cups and pin holes at stepped fits; the best becomes the starting fit | Done (0.61.0) |
| 53 | Clicks (ratchet) on hinges and swivels; ridges in ball sockets for a stiffer hold | Done (0.62.0) |
| 54 | Magnet pockets: standard sizes, open or under a skin, a matching pocket in the part it holds | Done (0.63.0) |
| 55 | Print Check: thin walls and overhangs coloured on the parts, with how much of each | Done (0.64.0) |
| — | Simplify by a number of triangles, with the STL file size | Done (0.64.1) |
| 56 | Rescale Design: to another scale, height or percentage, keeping joint sizes and wall thickness | Done (0.65.0) |
| 57 | Figure Guide: a posable see-through mannequin at a figure line's height and proportions | Done (0.66.0) |
| 58 | Hollow for resin: walls of a set thickness on any shape, drain holes (clicked or placed low), plugs | Done (0.67.0) |
| 59 | Surface stamps: rivets, bolts, screws, studs, vents, scales, stitches and custom shapes, raised or sunk, clicked or in rows | Done (0.68.0) |
| 60 | Panel lines: V, square or round grooves along lines clicked on the surface, open or closed | Done (0.69.0) |
| — | Shift for level, Ctrl for upright lines in Stamp rows and Panel Lines | Done (0.69.1) |
| 61 | Split for painting: alignment keys on Split by Mask's cuts, so pieces glue back one way | Done (0.70.0) |
| 62 | Kit layout: parts in rows on a sprue frame, gated, numbered, joined | Done (0.71.0) |
| — | Kit Layout: the tree's name on a tag under the frame | Done (0.71.1) |
| 63 | Parts list: size, volume, weight and cost per part, copied or saved as CSV or PDF | Done (0.72.0) |
| 64 | Tidy-up towards 1.0: help guide for the figure tools, speed check on a heavy sculpt, release notes | Done (0.73.0) |
| 65 | Pose Figure: a skeleton in one sculpted figure, fitted and posed (ready poses, typed angles, dragged balls), a step in the history | Done (0.74.0) |
| 66 | Paint: colours brushed onto one shape, kept as a step, shown, exported to 3MF for colour printers, split along by colour | Done (0.75.0) |
| 67 | Joint standards: a figure line's joint sizes by name, for Quick Socket, Quick Hinge and the Tolerance Test Print | Done (0.76.0) |
| 68 | Cut into Parts: a one-piece figure cut at its joints (skeleton fitted), ball joints or hinges put in, pieces named | Done (0.77.2) |
| 69 | Make Mould: a two-part mould box round a part, parted where it's widest, with registration keys, a pour spout and air vents | **Delivered for testing** |

Slices 39–41 close the gaps found in the [Phase 1 gate review](docs/PHASE1_GATE.md).

Waiting for a place: hand and accessory ports, a hand generator, joint cut-outs for a target angle, and more. See [docs/TODO.md](docs/TODO.md).

## Phase 2: Sculpt · Phase 3: Hybrid · Phase 4: Pro

See the spec doc. Slices will be listed here when Phase 1 nears its gate.
