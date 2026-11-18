# Test sheet: slice 44, Faster sculpting on big meshes (0.53.0 Beta)

Time needed: about 15 minutes. You'll need one of your own big meshes (a million
triangles or more, such as a figure buck you've imported as STL or 3MF).

## Get the build

Open the pull request **Slice 44: Faster sculpting on big meshes (0.53.0)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Sphere. Sculpt → **Sculpt**, Detail **High**. Drag the Draw brush (Size 3 mm) across it, then over the edge of the sphere so the ridge shows against the background | The bump follows the cursor with no lag, and the ridge shows on the outline | |
| 2 | Size 15 mm, drag quickly back and forth for a few seconds | It keeps up: no stutter or pauses part way through the stroke | |
| 3 | Size 1 mm with **Add detail where I sculpt** ticked; draw a few fine lines | Crisp lines; the triangle count at the top of the panel goes up; no lag | |
| 4 | Try Inflate, Smooth (Shift), Ctrl for pushing in, Mask, Clear mask, Invert mask, Mirror X | Each works as in 0.52, and the view shows each change straight away | |
| 5 | **Undo stroke** a few times, then sculpt again | Each stroke comes off; sculpting after it is still quick | |
| 6 | **Done**, then **Sculpt** the same shape again and add a stroke, then Done | Both sets of strokes are there. Do this two or three times: no crash | |
| 7 | Open one of your big meshes (a million triangles or more). **Sculpt** it with Draw at 3 mm, 10 mm and 25 mm | Smooth at every size: the brush keeps up with the cursor, as with the sphere | |
| 8 | Done. Help → **Open log folder**, open `forge3d.log`, and find the line "Sculpt: done, … dabs, average … ms each" | Please write down the number of triangles and the average. Under 8 ms leaves room for 60 frames a second | |
| 9 | Save, close and open the file again | The sculpting is all there and the shape is the same | |
| 10 | Open a file with sculpting saved by 0.52 | It opens with the same shape (the first open may take a little longer while it's worked out again) | |
| 11 | Smooth (whole shape), Simplify, Extrude Down and Split on a sculpted shape | All still work | |
