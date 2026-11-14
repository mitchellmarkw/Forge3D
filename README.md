# Forge3D

Parametric 3D modeling with sculpting, for Windows. The goal: the ease of Microsoft's
3D Builder, the precision of FreeCAD's parametric modeling, and Blender-style sculpting
in one document with one undo history.

> **Status:** early development (0.20). Phase 1, the 3D Builder replacement, is well
> along; sculpting is still to come. Expect rough edges, and keep copies of files you
> care about. See [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](CHANGELOG.md).

New to Forge3D? See the **[Quick Start](docs/QUICK_START.md)**: your first print in about ten minutes.

## What it can do so far

- **Shapes:** box, cylinder, sphere, cone, torus, tube, wedge, pyramid (oval versions
  too) and text. Move, turn and resize them with handles, with snapping.
- **Combine:** Merge, Subtract, Intersect, Split by a plane, Mirror, Group, Align.
  Results stay editable: change any part later and the result follows.
- **Edges and surfaces:** rounded, chamfered and bevelled edges; Emboss words that wrap
  round a surface, or a picture (a logo traced to an outline, or a photo as a
  relief); Shell (hollow out, with open faces).
- **Parametric design:** a history of steps with a rollback bar; a 2D sketcher with
  dimensions and rules; Pad, Pocket, Revolve, Groove; Holes (plain, counterbored,
  countersunk, tapped, metric sizes); Follow grooves and ridges along a path drawn on
  a flat or curved surface; linked patterns in rows, grids and circles.
- **Print:** Lay on Face (click a face to put the shape down on it) and Sit on Plate;
  export STL, 3MF and OBJ, or send straight to your slicer; import STL, 3MF
  and OBJ with automatic repair, and a fast mesh engine for big imported models.
- A ribbon with Build, Design, Sculpt and Print tabs; undo / redo for everything;
  Forge3D's own `.f3d` file format.

## Try it without building

Every push builds Forge3D on Windows automatically.

1. Open the **Actions** tab on GitHub and pick the latest green **Windows build** run.
2. Under **Artifacts**, download `Forge3D-windows-x64-…` and unzip it anywhere.
3. Run `Forge3D.exe`.

## Build it yourself (Windows)

You need **Visual Studio 2022** (with the "Desktop development with C++" workload, which
includes CMake, Ninja and vcpkg) and **Git**.

```powershell
git clone https://github.com/mitchellmarkw/Forge3D.git
cd Forge3D
```

Then either:

- **Visual Studio:** *File → Open → Folder…*, pick the `Forge3D` folder, choose the
  **Windows Debug** preset in the toolbar, and press **F5** with `Forge3D.exe` selected.
- **Command line** (from a *Developer PowerShell for VS 2022*):

  ```powershell
  cmake --preset windows-debug
  cmake --build --preset windows-debug
  ctest --preset windows-debug
  .\build\windows-debug\bin\Forge3D.exe
  ```

The first configure downloads and compiles Qt and OpenCASCADE through vcpkg. Expect
**one to two hours** the first time; later builds take seconds to minutes.

If CMake says it cannot find `vcpkg.cmake`, set `VCPKG_ROOT` to Visual Studio's copy
(for example `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg`) or to
your own clone of <https://github.com/microsoft/vcpkg>.

## Logs and crash reports

- Log: `%LOCALAPPDATA%\Forge3D\logs\forge3d.log` (*Help → Open log folder*).
- Crash dumps: `%LOCALAPPDATA%\Forge3D\crashes\*.dmp`.

Attach both when reporting a problem.

## Project layout

| Folder | What lives there |
| --- | --- |
| `src/core` | Geometry and document logic (OpenCASCADE for solids, Manifold for meshes; no Qt) |
| `src/view` | Camera and viewport maths (Qt GUI types, no window) |
| `src/app` | The Windows application: main window, menus, OpenGL viewport |
| `tests` | Catch2 unit tests |
| `docs/test-sheets` | Manual test steps for each delivered slice |

More detail in [ARCHITECTURE.md](ARCHITECTURE.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE). Models you create with Forge3D are yours;
the license covers the program, not your designs.

Forge3D is built on open-source libraries (Qt, OpenCASCADE, Manifold and others). See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the Windows download carries their
full license texts in its `licenses` folder.
