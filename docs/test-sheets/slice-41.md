# Test sheet: slice 41, Open PLY and glTF / GLB (0.50.0 Beta)

Time needed: about 10 minutes.

## Get the build

Open the pull request **Slice 41: Open PLY and glTF / GLB (0.50.0)**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

Sample files are in the repository under `docs/test-sheets/slice-41/` (on GitHub, open
each file and use the download button): `tower.glb`, `tower.ply`, and `tower.gltf`
with `tower data.bin` (keep those two together in one folder).

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | **Import** (Ctrl+I) | The file types offered include PLY and glTF / GLB | |
| 2 | Import `tower.glb` | A red six-sided tower with a pointed roof, **standing upright**, 20 mm across and 42 mm tall (check in Properties). No question about scale. Named "Tower" | |
| 3 | Import `tower.gltf` | A second red tower, named "Tower (glTF)" | |
| 4 | Move `tower data.bin` to another folder and import `tower.gltf` again | A message says its data file isn't beside it | |
| 5 | Import `tower.ply` | A green tower, the same size | |
| 6 | Drag `tower.glb` from Explorer onto the window | It comes in, as with Import | |
| 7 | Import a GLB or glTF you have or download (Sketchfab, Thingiverse or Printables downloads, or export one from Blender) | It comes in upright and at its real size, in its colours. Note anything odd (size, way up, missing parts) | |
| 8 | Import a PLY from a scanner or scanning app if you have one | It comes in (if it's only points, a message says to make it a mesh first). Note the size it comes in at | |
| 9 | Merge, Smooth or Export one of the imported towers | Works like any other imported shape | |
