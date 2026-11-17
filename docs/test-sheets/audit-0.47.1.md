# Test sheet: code audit (0.47.1 Beta)

Time needed: about 10 minutes. Mostly things that should now be refused politely.

## Get the build

Open the pull request **Code audit: damaged files and edge cases (0.47.1)**, open its
Windows build check, download the `Forge3D-windows-x64-…` artifact, unzip and run
`Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box 20 × 20 × 20. **Edges**, choose Rounding, pick an edge, Size **500**, Apply | At once, a message that the size is bigger than the whole shape; Forge3D isn't left busy. Cancel the tool | |
| 2 | Add a Cylinder, diameter 6, height 300. **Thread** its round face with pitch **0.1** | A message that the pitch is too fine for this length (over 2000 turns). Pitch 1 works | |
| 3 | Add a **Text** shape and paste about 130 letters into its Text box in Properties | The box stops taking letters a little past 120; with more than 120 letters (not counting spaces) a message says up to 120 at a time | |
| 4 | In Notepad save a file `bad.obj` holding the three lines `v 0 0 0`, `v 1 0 0`, `f a b c`. **Import** it | "Could not import bad.obj: This OBJ file is damaged (line 3)." Nothing else happens | |
| 5 | Save `bad.stl` holding just `hello`. Import it | "This doesn't look like an STL file (or it is damaged)." | |
| 6 | Set backups to every 2 minutes (Edit → Backup Copies). Add a box, wait 2 minutes, then delete the box and wait another 2 minutes. End Forge3D in Task Manager and start it again | No offer to recover a backup | |
| 7 | Quick round: add a few shapes, Merge, Split with pegs, Undo / Redo several times, Save, close, reopen | Everything as before | |
