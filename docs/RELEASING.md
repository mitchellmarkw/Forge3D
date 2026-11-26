# Making a release

Releases are built and published by the Windows build (`.github/workflows/windows.yml`)
when a version tag is pushed.

1. On `main`, make sure `CMakeLists.txt` has the new version (`project(Forge3D VERSION
   x.y.z)`) and label (`FORGE_RELEASE_LABEL`: "Beta", or "" for a plain release), and
   that `CHANGELOG.md` describes it.
2. Write the release notes as `docs/release-notes/vX.Y.Z.md` (shown on the release page)
   and merge them.
3. On GitHub: **Actions → Windows build → Run workflow**, branch **main**, tick
   **Publish a GitHub release of this version**, and **Run workflow**.

   (Or tag the commit yourself and push the tag: `git tag v0.25.0` then
   `git push origin v0.25.0`.)

4. The build runs as usual, then makes the tag `vX.Y.Z` and publishes a release named
   "Forge3D 0.25.0 Beta" with `Forge3D-0.25.0-Setup.exe` and
   `Forge3D-0.25.0-portable.zip`. It refuses if that release already exists (bump the
   version first), a pushed tag doesn't match CMakeLists.txt, or the notes are missing.

Every build (not only tags) also uploads the installer as an artifact, to try before
releasing. Forge3D's Help → Check for Updates reads the list of published releases, so
people see the new version as soon as it's published (drafts are ignored).

The installer isn't code-signed, so Windows SmartScreen warns on first run ("More info",
"Run anyway"). Signing can be added later in the "Installer and portable zip" step.

## Forge3D Scenery

Forge3D Scenery (`src/scenery`) lives in the same repository and is built by the same
workflow, but is released on its own:

- Its version is `SCENERY_VERSION` (and `SCENERY_RELEASE_LABEL`) in `CMakeLists.txt`; its
  changelog is `docs/scenery/CHANGELOG.md`.
- Release notes go in `docs/scenery/release-notes/scenery-vX.Y.Z.md`.
- **Run workflow** with **Publish a Forge3D Scenery release** ticked (or push a tag
  `scenery-vX.Y.Z`). It publishes "Forge3D Scenery X.Y.Z Beta" with
  `Forge3D-Scenery-X.Y.Z-Setup.exe` and `Forge3D-Scenery-X.Y.Z-portable.zip`, not marked
  "Latest" (Forge3D's releases stay at the top).
- Each app's Check for Updates looks only at its own tags (`v…` for Forge3D,
  `scenery-v…` for Scenery), so neither offers the other's releases.
