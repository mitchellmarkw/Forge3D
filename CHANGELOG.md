# Changelog

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
