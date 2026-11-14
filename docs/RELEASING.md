# Making a release

Releases are built and published by the Windows build (`.github/workflows/windows.yml`)
when a version tag is pushed.

1. On `main`, make sure `CMakeLists.txt` has the new version (`project(Forge3D VERSION
   x.y.z)`) and label (`FORGE_RELEASE_LABEL`: "Beta", or "" for a plain release), and
   that `CHANGELOG.md` describes it.
2. Write the release notes as `docs/release-notes/vX.Y.Z.md` (shown on the release page)
   and merge them.
3. Tag that commit and push the tag:

   ```
   git tag v0.25.0
   git push origin v0.25.0
   ```

4. The build runs as usual, then publishes a release named "Forge3D 0.25.0 Beta" with
   `Forge3D-0.25.0-Setup.exe` and `Forge3D-0.25.0-portable.zip`. It refuses if the tag
   and the version in CMakeLists.txt differ, or the notes are missing.

Every build (not only tags) also uploads the installer as an artifact, to try before
releasing. Forge3D's Help → Check for Updates reads the list of published releases, so
people see the new version as soon as it's published (drafts are ignored).

The installer isn't code-signed, so Windows SmartScreen warns on first run ("More info",
"Run anyway"). Signing can be added later in the "Installer and portable zip" step.
