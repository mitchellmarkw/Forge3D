# Test sheet: slice 63, Parts list (0.72.0 Beta)

Time needed: about 5 minutes.

## Get the build

Open the pull request **Slice 63: Parts list (0.72.0)**, open its Windows build check,
download the `Forge3D-windows-x64-…` artifact, unzip and run `Forge3D.exe`.

## Checks

| # | Do this | You should see | Result |
| --- | --- | --- | --- |
| 1 | Add a Box, Cylinder and Sphere; nothing selected; **Tools → Parts List…** | A table: each with size 20.0 × 20.0 × 20.0, volume 8.00 / 6.28 / 4.18 cm³, weight and cost; a Total row. Material (label on top of its drop-down) PLA | |
| 2 | Material: Standard resin | Density 1.12, Fill 100 %; weights go up | |
| 3 | Type a different Density | Material shows "Other" | |
| 4 | Change the price and currency sign (e.g. £ 35) | The costs follow | |
| 5 | Make a kit of them (Kit Layout, joined), then Parts List with "List the parts inside joined shapes" | The kit, its frame and parts indented under it; the total counts the kit once | |
| 6 | **Copy**, paste into a spreadsheet | The table in cells | |
| 7 | **Save as Spreadsheet…**, open it | The table, × and ³ shown right | |
| 8 | **Save as PDF…**, open it | A printable page with the table and the material line | |
| 9 | Close and reopen | The settings are as you left them | |
