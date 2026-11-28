# Forge3D Scenery changelog

## 0.7.1 Beta — The model scale, followed (2026-11-27)

- Fixed: changing the **Model scale** while designing a building, rock, terrain piece or tree did nothing to it.
  Now it's made again at the new scale at once (sizes are real-world, so it gets bigger or smaller on the plate),
  with any notes for that scale (bricks too small to print, and so on); the status bar says so.
- Changing the scale with scenery already made at another scale asks whether to remake it at the new one: Yes
  remakes each piece where it stands (one Undo takes it all back; any that can't be made at that scale are
  listed and stay as they were); No leaves them as they are, and only new scenery uses the new scale.

## 0.7.0 Beta — Rough stone walls (2026-11-27)

- Two new wall materials:
  - **Rough-hewn stone**: roughly squared, rock-faced stones in courses of varying height, each stone a different
    length, standing out of the mortar with a rough, domed face.
  - **Fieldstone (irregular)**: rounded stones of every shape in wide mortar lines, like a "giraffe" stone house.
    One size sets them (the average stone's).
  - Both with mortar (set back) or laid dry; another stone pattern with each building's number; any building
    shape; doors and windows cut through as usual. Mortar lines too fine to print are widened; stones too small
    to print at the scale give plain walls (with a note).
- The building's preview shows its material's colour (it was always brick red).

## 0.6.0 Beta — Building shapes (2026-11-26)

- **Shape** (Building, under Walls): **Rectangle**, **L-shape**, **T-shape**, **U-shape** or **Courtyard** (wings
  all round an open middle). The walls follow the shape, joined at every corner: bricks bonded round outside
  corners, logs crossing, planks with corner boards.
  - Each shape's wings have their own sizes (an L's front wing depth and side wing width, a T's stem, a U's arms, a
    courtyard's wings), kept buildable: each wing at least 2.5 walls thick, an open middle at least 500 mm.
  - **Drag the orange posts** at the footprint's corners in the view: the outside ones size the building, the
    inside ones its wings.
  - Each wing gets its own roof (flat, gable or hip), joined where they meet.
  - Doors and windows go on any wall, a courtyard's inner walls too; the list names the wall ("left (6)",
    "inner front (7)"). Changing the shape takes the doors and windows off (its walls are others).

## 0.5.0 Beta — Trees (2026-11-26)

- **New Tree** (Scenery tab): a tree from real-world sizes, made at the model scale.
  - Kinds: **Broadleaf** (spreading branches, a rounded crown), **Conifer** (a straight trunk, tiers of drooping
    branches, a cone), **Dead tree** (bare, gnarled branches, some broken off) and **Palm** (a curved trunk,
    arching fronds).
  - Height, crown width, trunk diameter; **Branches** (how many; a conifer's tiers, a palm's fronds); **Lean**.
  - **Leaves**: lumpy foliage clumps, made to print (untick for a bare tree); **Foliage** sets how full they are.
  - It stands on flared roots, cut flat. Branches too thin to print are thickened to 2.5 times the smallest
    detail (at least 0.8 mm), so the thinnest twigs are a little heavy at small scales.
  - A new tree (or another kind) is sized to fit under your printer.
  - **New Shape**: another of the same kind and size; its **Number** makes the same one again.
- **Change** (or double-click in Objects) now changes trees too.

## 0.4.0 Beta — Terrain (2026-11-26)

- **New Terrain** (Scenery tab): a terrain piece from real-world sizes, made at the model scale.
  - **Base**: a rectangle, or round or oval; width and depth; base thickness; a plain edge or a bevelled one (a
    sloped plinth, how far it slopes in).
  - **Ground**: rolling hills (height and size), roughness; **Level at the edges** so pieces put side by side meet
    at the same height. **New Hills** for others; their **Number** makes the same ones again.
  - **Craters** (a bowl with a raised rim), **crags** (rock outcrops pushed up through the ground) and **paths**
    (a flattened track, sunk a little, its banks sloping off). **Add Crater** or **Add Crag**, then click on the
    ground; **Add Path**, then click where it starts and where it ends. Click one to pick it; drag it to move it
    (a path's end, near it); set its size on the right; Remove (or Delete).
  - **Scatter**: so many craters, crags and paths at random (again for other places); the ones you placed or
    changed stay. **Clear Scattered** takes the scattered ones out.
  - It shows at once roughly, then in full a moment later.
- **Change** (or double-click in Objects) now changes terrain too.

## 0.3.0 Beta — Rocks and boulders (2026-11-26)

- **New Rock** (Scenery tab): design a rock from real-world sizes, made at the model scale.
  - Kinds: **Boulder** (rounded and worn), **Crag** (broken rock: flat faces, sharp edges), **Slab** (a flat stone)
    and **Pile of rocks** (2 to 40 rocks heaped together, the big ones lower and in the middle).
  - Length, width and height; **Roughness** (from smooth to lumpy and pitted); **Layers** (bedding lines across
    it, as in sandstone or shale) and how far apart they are.
  - **Sunk into the ground**: its base cut flat that far up, so it sits on the ground (and prints flat).
  - **New Shape**: another rock of the same kind and size. Its **Number** makes the same one again.
  - **Smallest detail**: no finer surface is made than your printer can print.
  - It shows at once roughly, then in full a moment later.
- **Change** (was Change Building), or double-click in Objects: changes the picked building or rock, made again
  where it is. Undo takes it back.
- Fixed: changing a building kept its old settings in the file, so changing it a second time (or after opening the
  file) started from the first ones.

## 0.2.0 Beta — Building designer (2026-11-26)

- **New Building** (Scenery tab): design a building from real-world sizes, made at the model scale.
  - **Walls** of **Brick** (a standard 194 x 92 x 57 mm brick, running bond, the corners bonded), **Stone blocks**
    (courses of blocks of varied lengths), **Logs** (stacked round logs, their ends crossing past the corners) or
    **Wood planks** (clapboard on a plain wall, with corner boards). Width, depth, wall height and thickness.
  - **Mortar** (bricks and blocks): the units spaced by the joint, the mortar set back from the face; or laid
    without, the joints shown as fine lines.
  - **Roof**: none, flat, gable (the ridge along the longer side) or hip; pitch and overhang; its own shape (it
    lifts off) or joined to the walls.
  - **Doors and windows**: Add Door or Add Window, then click on a wall; drag one to move it (even round a corner);
    set its width, height and sill on the right; Remove (or Delete). Frames, a window's sill and glazing bars, a
    door's leaf. They're kept clear of the corners and of each other.
  - **Smallest detail** your printer makes: mortar finer than it is widened (and you're told); bricks, blocks, logs
    or planks too small to print leave the walls plain.
  - It shows at once in a plain version, then in full a moment later.
- **Change Building** (or double-click it in Objects): every setting, doors and windows included, can be changed
  and the building made again where it is. Undo takes it back.
- Objects: click a shape to pick it; Edit → Delete removes it.

## 0.1.0 Beta — Groundwork (not released on its own)

- The companion app to Forge3D, for scenery: buildings, trees, rocks and terrain (coming next).
- Opens, shows, saves and exports Forge3D's .f3d models (they open in either app), with the same 3D view and
  view cube, and the printer plate set in Forge3D.
- **Model scale** on the Scenery tab: the scale the scenery tools will build to (1:6 to 1:220, 1:60 among them, or
  your own, like 1:76 or 1:87.1). Remembered.
- Light and dark themes (shared with Forge3D), Check for Updates (Scenery's own releases only).
