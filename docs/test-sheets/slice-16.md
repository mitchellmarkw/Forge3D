# Test sheet: slice 16, the first public release (0.25.0 Beta)

Time needed: about 15 minutes.

## Get the build

Open the pull request **Slice 16: First public release (0.25.0 Beta)** and its Windows
build check. This time there are **two** downloads under Artifacts:

- `Forge3D-windows-x64-…`: the usual zip (with the test sheets);
- `Forge3D-Setup-0.25.0-…`: unzip it to get `Forge3D-0.25.0-Setup.exe` and
  `Forge3D-0.25.0-portable.zip`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Run `Forge3D-0.25.0-Setup.exe` | Possibly "Windows protected your PC" (More info → Run anyway). Then a choice: install for me only, or for all users | |
| 2 | Install for you only, with "Open .f3d files with Forge3D" ticked and the desktop icon ticked | It installs without asking for an administrator password, and offers to start Forge3D | |
| 3 | Start it from the Start menu | Forge3D with its blue cube icon on the taskbar and window; the title says "0.25.0 Beta" | |
| 4 | The tour runs (first run of this install's settings, if you haven't seen it), then Forge3D asks "Should Forge3D check for a newer version each time it starts?" | Two buttons: "Yes, check at start" / "No, I'll check myself" | |
| 5 | Choose Yes | Nothing else happens (there's no newer release); Help → Check for Updates at Start is ticked | |
| 6 | Help → **Check for Updates...** | "You have the newest Forge3D (0.25.0 Beta)." | |
| 7 | Close and start again | It doesn't ask again | |
| 8 | Save a model as a .f3d, close Forge3D, double-click the file in Explorer | Forge3D opens with the model; the file shows Forge3D's icon | |
| 9 | Help → About | "Forge3D 0.25.0 Beta" | |
| 10 | Windows Settings → Apps → Forge3D → Uninstall | It's removed: no Start menu entry or desktop icon, and .f3d files no longer open in it | |
| 11 | Unzip `Forge3D-0.25.0-portable.zip` somewhere and run `Forge3D.exe` | It runs without installing; there's no test-sheets folder in it | |

## Send back

"All pass" or the failures (with the number), and `forge3d.log` if anything failed.
When it passes and is merged, tell me and I'll tag v0.25.0 to publish the release (or do
it yourself: docs/RELEASING.md).
