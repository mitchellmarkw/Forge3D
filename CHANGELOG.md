# Changelog

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
