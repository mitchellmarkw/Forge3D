# Third-party notices

Forge3D is free software under the GNU General Public License, version 3 or later (see
`LICENSE`). It is built with, and the Windows download includes, the open-source
libraries below. Each remains under its own license.

The Windows download has a `licenses` folder holding the full license and copyright
text of every library it includes, one sub-folder per library (copied from the build,
so it always matches what's shipped). This file is a summary.

## Libraries the program uses directly

| Library | What Forge3D uses it for | License | Source |
| --- | --- | --- | --- |
| Qt 6 (Qt Base) | Windows, menus, panels, OpenGL view | LGPL-3.0 | <https://www.qt.io/download-open-source> |
| Open CASCADE Technology | Solid geometry (shapes, booleans, fillets, sweeps) | LGPL-2.1 with the Open CASCADE exception | <https://dev.opencascade.org> |
| Manifold | Mesh booleans for imported models | Apache-2.0 | <https://github.com/elalish/manifold> |
| miniz | Reading and writing 3MF (zip) files | MIT | <https://github.com/richgel999/miniz> |
| JSON for Modern C++ (nlohmann/json) | The `.f3d` file format | MIT | <https://github.com/nlohmann/json> |

Qt and Open CASCADE are linked dynamically: their DLLs sit beside `Forge3D.exe` and can
be replaced with your own builds of the same versions, as the LGPL allows. The source of
every library is available from the links above, and the exact versions Forge3D builds
against are pinned in `vcpkg.json` (through vcpkg's `builtin-baseline`).

## Libraries those depend on

Qt, Open CASCADE and Manifold bring in further libraries, such as FreeType, HarfBuzz,
libpng, libjpeg-turbo, PCRE2, double-conversion, zstd, zlib and oneTBB. Their licenses
are in the `licenses` folder of the download too.

## Microsoft Visual C++ runtime

The download includes the Visual C++ runtime DLLs (`msvcp140.dll`, `vcruntime140.dll`
and others), redistributed under the Microsoft Visual Studio license terms, so Forge3D
runs on PCs without the Visual C++ Redistributable installed.

## Used only to build and test (not shipped)

Catch2 (BSL-1.0), CMake, Ninja and vcpkg.
