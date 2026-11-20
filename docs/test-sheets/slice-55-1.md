# Test sheet: 0.64.1, Simplify by a number of triangles

Time needed: about 3 minutes.

## Get the build

Open the pull request **0.64.1: Simplify by a number of triangles**, open its Windows
build check, download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Import a heavy mesh (one of your sculpts or scans), select it, Sculpt tab → **Simplify** | Properties: Keep 50.0 %, **Triangles** (half the count), and "As an STL file: about … MB (was … MB)" | |
| 2 | Type a number into **Triangles** (say 20,000) | It simplifies to about that many; Keep shows the matching %; the file size updates | |
| 3 | Set **Keep** to 0.5 % | Triangles follows; the shape is much lighter but still its shape | |
| 4 | Undo, Redo | The counts and the size follow each step | |
| 5 | Select a plain shape (a box) | No Keep, Triangles or file-size lines | |
