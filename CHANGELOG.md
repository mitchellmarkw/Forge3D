# Changelog

## 0.70.0 Beta — Slice 61: Split for painting (alignment keys) (2026-11-20)

- **Split by Mask** (Design tab) can add **alignment keys** on each cut, so pieces split
  off for painting glue back only one way:
  - A three-sided peg (its point aims one way, so it can't go in turned) stands on the
    painted piece's cut, half sunk in; the rest gets the matching hole, bigger by the
    **Key fit** all round (0.1 mm to start: resin; about 0.2 mm for filament).
  - Each key is sized to its cut (a third of the way to the edge, at most 3 mm, reaching
    1.5 times that into the other piece); cuts too small for one are left plain.
  - On by default and remembered. Changing the mask later (double-click either piece)
    can turn the keys on or off, or change the fit; both pieces change together.
  - The history says so: "the masked part (24 dabs, keyed)".
- Files saved now need Forge3D 0.70 or later to open (format 37).
- 320 unit tests (1 new: an arm's hand end cut off with a key: where it goes, the peg
  and the hole's sizes, saving, and turning the keys off).

## 0.69.1 Beta — Straight lines for stamps and panel lines (2026-11-20)

- **Shift** keeps a line **level** and **Ctrl** keeps it **upright**:
  - Panel Lines: hold it as you click the next point; the piece from the last point
    stays level with it (the same height all the way), or goes straight up or down
    from it as seen from where you're looking.
  - Stamp: hold it while dragging a row.
  - Either way the line stays in one flat plane and follows the surface across it, so
    on a round shape a level line runs round it like a hoop.

## 0.69.0 Beta — Slice 60: Panel lines (2026-11-20)

- **Panel Lines** (Features tab, Surface group): click points along the selected
  shape's surface to cut a thin groove that follows it, for armour plates, vehicle
  panels and scale models. Works on any shape (sculpts and imported meshes too).
  - Between clicks the line keeps to the surface, round curves and over edges.
  - **End Line** starts another; **Close Loop** joins it back to its first point (round
    a panel). **Undo Point**, **Remove Line** and **Clear**.
  - **Groove shape**: V (sharp, like a scribed line), Square or Round; **Width**
    (0.6 mm to start) and **Depth** (0.5 mm); remembered.
  - The grooves show red, seen through the shape, until Apply.
  - A step in the history ("3 panel lines, V 0.6 × 0.5 mm"): double-click to add lines
    or change the groove; Remove the panel lines gives the shape back. Rescale Design
    moves the lines and, evenly scaled, grows the groove.
- Files saved now need Forge3D 0.69 or later to open (format 36).
- 319 unit tests (3 new: each groove shape's size; a corner, a closed loop and a groove
  round a ball; saving and checks).

## 0.68.0 Beta — Slice 59: Surface stamps (2026-11-20)

- **Stamp** (Features tab, Surface group): small details on the selected shape's
  surface, any shape (sculpts and imported meshes too).
  - **Stamps**: Rivet, Hex bolt, Screw (slotted), Stud, Vent (three slots), Scale,
    Stitch, and **your own custom shapes** (the ones saved with Add → Custom Shapes,
    fitted to the size and height).
  - **Raised** (standing out) or **Sunk** (cut in). Size, Height (each kind starts at
    its usual height), Turn, and Row spacing.
  - **Click** the shape for one stamp; **drag** across it for a row, spaced evenly so
    the last lands where you let go, each one set onto the surface and lined up along
    the row (stitches turned 30° look sewn). Remove Last takes away the last click or
    row; Clear takes them all.
  - Gold previews show raised stamps, red ones sunk stamps, until Apply.
  - Each stamp has a stem the other way, so it meets a curved surface without a gap.
  - A step in the history ("12 rivets Ø3, raised"): double-click to add more, move
    them, or change the size; Remove the stamps gives the shape back.
- Files saved now need Forge3D 0.68 or later to open (format 35).
- 316 unit tests (4 new: every kind's shape and size; rivets raised and sunk; rows,
  turning and a custom shape; saving and checks).

## 0.67.0 Beta — Slice 58: Hollow with drain holes (2026-11-19)

- **Hollow** (Features tab, beside Shell): for resin printing, takes the inside out of
  the selected shape, leaving walls of a set thickness (2 mm to start). Unlike Shell it
  works on **any** shape: sculpts, imported meshes, everything.
  - **Drain holes**: click the shape where each goes, or **Place Them for Me** (two at
    the lowest points as it sits, apart from each other). They show as red rods until
    Apply. **Remove Last** and **Clear**; hole size 3 mm to start.
  - **Make plugs to close the holes**: a short peg per hole, beside the model, the
    hole's size less your resin peg fit (from the tolerance test), as long as the wall
    is thick. They're joint parts (Rescale keeps their size).
  - A step in the history ("2 mm walls, 2 drain holes Ø3"): double-click it to change
    the walls or the holes; Remove the hollowing gives the solid back.
  - Parts nowhere thicker than two walls stay solid. Accurate to about a third of the
    wall thickness; a big part takes a few seconds.
  - Rescale Design keeps the walls (Keep wall thickness) and the holes (Keep joint
    sizes) if asked.
- Files saved now need Forge3D 0.67 or later to open (format 34).
- 312 unit tests (4 new: a box's walls and a drain hole; a sphere, and a slab too thin
  to hollow; holes placed low, plugs and checks; saving and rescaling).

## 0.66.0 Beta — Slice 57: Figure Guide (2026-11-19)

- **Figure Guide** (View tab, Pictures; and the View menu): a see-through mannequin on
  the plate to build a figure to, like a 3D reference image. Never printed or exported.
  - **Figure size**: a figure line's height (3.75, 5, 6, 7 or 12 inches) or any height
    typed in mm.
  - **Proportions**: Stylised (6 heads), Realistic (7.5), Heroic (8), Fashion (9) or
    any number of heads; the panel says how tall a head is.
  - **Where it stands**: X, Y and a turn. A new guide stands beside the model, and the
    view moves to show all of it.
  - **Pose**: ready poses (Standing, A-pose, T-pose, Walking, Action), then any joint:
    neck (nod, turn), waist (bend, twist), shoulders and hips (forward, out), elbows
    and knees (bend). It stays standing on the plate as the legs bend.
  - Show or hide it, or remove it. Every change can be undone (a run of changes to one
    setting is one step). Kept in the file: files saved now need Forge3D 0.66 or
    later to open (format 33).
- 308 unit tests (3 new: proportions from heads; posing, standing on the plate, turning
  and ranges; undo, merging and the file).

## 0.65.0 Beta — Slice 56: Rescale Design, keeping joint sizes (2026-11-19)

- **Tools → Rescale Design**: move a design to another scale.
  - Everything on the plate, or just the selected shapes.
  - **From one scale to another** (1:12 to 1:18, say), **to a new height**, or **by a
    percentage**. The parts grow or shrink together about the middle of their
    footprint, so what stands on the plate stays there. One undo step.
  - **Keep joint sizes**: joint parts move with the design but keep their size, and
    the joints cut into the shapes keep theirs (their spots move with the shape):
    ball and peg sockets, magnet pockets, Quick Hinge hinges and swivels, threads. The
    dialog says how many it found, and warns to check they still fit when shrinking.
  - **Keep wall thickness**: shapes hollowed with Shell keep their walls.
- **Joint parts**: a new tick box in Properties, **Joint part (keeps its size when
  rescaled)**, for any ball, peg or pin you make yourself. Already ticked for the
  balls, pegs and stand-in magnets from Quick Socket, Quick Hinge's tab, link and pin
  pieces, and Split's pegs, key holes and loose pins (made from now on).
  Saved in the file; older versions ignore it.
- 305 unit tests (4 new: a design shrinking together; a joint part kept and moved; a
  peg inside a combined shape and a socket step kept; Shell walls kept, and the joint
  part saved and undone).

## 0.64.1 Beta — Simplify by a number of triangles (2026-11-19)

- **Simplify** (Sculpt tab) was already there; its Properties now also take a number
  of **Triangles** to aim for, alongside Keep (%), and show how big an STL file it
  would make, then and before. Keep can now go down to 0.1 %, for very heavy sculpts.
- The to-do item "Simplify a mesh" is done by this.

## 0.64.0 Beta — Slice 55: Print Check (2026-11-19)

- **Print Check** (the Check group, next to Measure, on every tab): colours every part
  on the plate by what may not print well, the way it sits now. Nothing is changed.
  - **Thin walls** (red): places thinner than a set thickness (0.8 mm to start, up to
    5 mm), measured straight in from each face to the other side: fingers, cape edges,
    weapon tips.
  - **Overhangs** (blue): faces that lean out past a set angle from upright (45° to
    start), so they'll need supports. Faces resting on the part's bottom don't count.
  - Each can be turned off; the settings are remembered.
  - The panel lists each part with a problem: how much of it is too thin (and the
    thinnest place) and how much needs supports. Big meshes are measured in the
    background, on every core; overhangs show at once.
  - Done or Esc closes it. To try another way up, turn the part and check again.
- Esc now closes an open tool even when nothing is selected.
- 301 unit tests (4 new: a thin slab; a hollow box's walls; overhangs on and off the
  plate, tipped and at two angles; two thousand parts measured over the cores).

## 0.63.0 Beta — Slice 54: Magnet pockets (2026-11-19)

- **Quick Socket → Socket for: A magnet**: a round pocket for a magnet, for swappable
  heads, hands and accessories.
  - Standard sizes to pick (6×2, 5×1, 5×2, 4×2, 3×1, 3×2, 8×3, 10×2 mm) or **Other
    size**. The pocket is the magnet plus the fit, deep enough for the magnet to sit
    flush.
  - **Hidden under a skin**: a closed pocket under a thin skin (0.6 mm to start), for
    dropping the magnet in during a pause in the print.
  - **A matching pocket in the part it holds**: pick the part against the face (a head
    on a neck) and it gets its own pocket, lined up, in the same undo step. A reminder
    about the poles comes after.
  - **Add a Stand-in Magnet** adds a magnet-sized cylinder to see it in place.
  - The fit starts from the peg fit recorded with the tolerance test. No relief cuts.
- Pose Test lists magnets as joints that hold a part on without turning.
- Files with magnet pockets need Forge3D 0.63 or later to open (format 32).
- 297 unit tests (3 new: an open pocket's size; a buried pocket's skin left whole; the
  matching pocket found on the part against the face, Pose Test's joint, and the file).

## 0.62.0 Beta — Slice 53: Clicks and ridges (2026-11-19)

- **Clicks** (a ratchet) in Quick Hinge: **Clicks a turn** and **Click depth**.
  - Hinges: V-shaped ridges on both faces of the tab (or a double hinge's link), round
    the pin, and matching grooves in the fork's arms. Straight, and at every click, the
    ridges sit in the grooves with the fit all round; in between they press on the arms
    by the click depth less the fit, so the joint clicks round and holds a pose.
  - Swivels: ridges along the peg and grooves along the bore (the slit across the peg
    lets it squeeze round).
  - The click depth must be more than the fit; a tab or peg too small for that many
    clicks says so. Remembered for next time.
- **Ridges** in Quick Socket's ball sockets: **Ridges** and **Ridge height**. Ridges are
  left standing inside the cup, between any relief cuts, pressing on the ball by their
  height less the fit, for a stiffer hold.
- **Pose Test** rests a joint with clicks on its clicks (dragging or typing snaps to the
  nearest), and Find Range counts in clicks.
- The history says "8 clicks" or "6 ridges". Files with clicks or ridges need Forge3D
  0.62 or later to open (format 31).
- 294 unit tests (6 new: a hinge's ridges clear at every click and pressing between;
  a swivel's likewise; the settings checked; a ball socket's ridges pressing on the
  ball; clicks and ridges kept in the file; Pose Test's clicks).

## 0.61.0 Beta — Slice 52: Tolerance Test Print (2026-11-19)

- **Tools → Tolerance Test Print...**: find the fit that suits your printer, once.
  - **1. Make the test**: bars of holes for a round peg, snap-in cups for a ball, and
    holes for a hinge pin, at stepped fits (0.10 to 0.30 mm every 0.05 to start;
    from, to and step can be changed, up to 12), each labelled with raised digits
    (".15" is 0.15 mm of room all round), plus two test pegs, a ball on a stalk and two
    pins. The holes and cups are cut exactly as Quick Socket cuts them. Sizes can be set
    to the ones you use, and **Add to the Plate** puts it all beside what's there.
  - **2. Record what fit best**: for each printer (resin, FDM), the peg, ball and hinge
    pin fits that worked. **Use These Fits**: Quick Socket and Quick Hinge start from
    them whenever that printer is chosen (and when the socket's kind changes), and
    Split for Printing's pegs use the peg fit. The fit can still be changed each time.
- 288 unit tests (3 new: the fits and labels; the bars and pieces, closed, on the plate
  and apart; a test peg clearing each hole by its labelled fit, and a fatter one not).

## 0.60.0 Beta — Slice 51: Pose Test (2026-11-19)

- **Pose Test** (Features → Holes and Joints): turn a figure's joints to see how far
  they go before parts hit, without changing the parts.
  - The joints are found from the document: every Quick Hinge fork (each fork of a
    double hinge, with the link between them) and every Quick Socket (a ball turns any
    way; a round peg turns round its axis; a keyed peg just holds). The part that turns
    in each is the tab's part, or else the biggest part round the pin or ball (so not
    the pin itself).
  - **Drag a part** in the view to turn its joint (a hinge round its pin; a ball round
    your line of sight), or choose a joint in the list and type its angle (a ball:
    Bend, Bend sideways and Twist). Parts further out move with it; pins move with the
    part they're held in.
  - **Parts that hit** are tinted red, with the overlap shown bright red, and named in
    the panel. It's checked as soon as you stop moving, in the background, so dragging
    stays smooth on big meshes.
  - **Find Range**: turns a hinge or swivel each way from straight until parts hit and
    says how far it went (to the degree). If it hits straight away, it says why.
  - **Hold Still**: the biggest part is held still to start (a figure's body); click a
    part and Hold Still to hold another.
  - **Saved poses**: Save Pose... keeps the joints' angles by name, in the file (undo
    works); choose one to show it again; Delete forgets it. **Back to the Build Pose**
    straightens everything; **Done** shows the parts as built again.
- Files with saved poses need Forge3D 0.60 or later to open (format 30).
- 285 unit tests (5 new: a hinge's joint and its parts, the pin riding along; parts that
  hit, and the range to just before they do; a ball's three angles; a double hinge's
  link between two forks; saved poses kept, undone and saved in the file).

## 0.59.1 Beta — Scale Calculator, mask colours (2026-11-19)

- **Tools menu → Scale Calculator**: a real-world size in mm, cm, m, inches, feet or
  yards, at a scale of 1 : X, gives the size on the model in the unit you choose (a 6 ft
  person at 1:60 is 30.48 mm). It stays open beside the window while you work,
  remembers what you typed, and **Copy** puts the answer on the clipboard to paste
  into a size box. No ribbon tab.
- **Mask colours**: while painting a mask (Split by Mask, and sculpting's Mask brush)
  the shape is shown in two plain colours, black paint on yellow, evenly lit, so a
  shadow never looks like paint. Change either colour (or **Swap** or **Reset**) under
  **Colours while painting** in the Split by Mask panel; they're kept between runs.
- Reference Images' "Size from two points" takes yards too.
- 280 unit tests (the scale sums checked: 6 ft at 1:60 is 30.48 mm; yards).

## 0.59.0 Beta — Slice 50: Quick Hinge (2026-11-19)

- **Quick Hinge** (Features → Holes and Joints): a joint at the spot you click on the
  selected shape, sized to fit once printed.
  - **Single hinge**: a fork (a slot with a pin hole across it) cut into this part, and
    a tab with a rounded end that turns in it on a pin.
  - **Double hinge**: a fork in each part and a link between them with a pin at each
    end, so it bends further (knees, elbows).
  - **Swivel**: a bore with a groove at the bottom, and a peg with a lip that snaps into
    the groove (a slit across its end lets the lip squeeze through), for waists and
    wrists.
  - **The other half**, chosen each time: a **new shape in place** (the tab or peg, to
    Merge onto the other part or keep apart), or **made from another part** (pick it:
    its end is trimmed into the tab, or the peg goes on it, standing just clear of the
    fork's face). For a double hinge the link is always a shape of its own; the other
    part can get its fork too, lined up with the link.
  - **Pin**: printed (the default; a pin shape is added, cut to the part's width there),
    1.75 mm filament, or a wire or rod of a typed size. The holes are made bigger by the
    fit: snug in the fork's arms; in the tab, snug when **Stiff** (holds a pose) or with
    room to turn freely.
  - **Bends** both ways or one way only (the slot is cut deep on that side), **Turn**
    (which way the pin lies; +90° turns it a quarter), **Reach** (how far the slot and
    pin hole go out), **Center on the face**, and the printer and fit, remembered for
    next time.
  - One undo step for the lot. Double-click either half to change it: the other half
    changes with it. Big shapes show the cut in red while you set it up.
- Files with Quick Hinge need Forge3D 0.59 or later to open (format 29).
- 280 unit tests (6 new: the settings checked; the tab and pin clear of the fork and
  bending 90° both ways, or stopped one way; another part made into the tab, saved and
  opened again as a pair; a double hinge's link clear of both forks; a swivel's peg
  seated clear and caught by its lip; scaling keeps a filament pin's size).

## 0.58.0 Beta — Slice 49: Reference Images (2026-11-18)

- **Reference Images** (View → Pictures, or the View menu): a photo, drawing or model
  sheet for each side (front, back, top, bottom, left, right), standing behind the
  model to build to.
  - **Add Picture…** places it on the plate behind the model, a bit bigger than it,
    and turns the view to that side. **Replace Picture…** keeps its size and place.
  - **Size from two points**: type a length you know, click its two ends on the
    picture, and the picture is sized so they are that far apart. The length can be
    real with the model's scale (4.5 m at 1:48 is 93.75 mm on the model) or already to
    scale (91.2 mm, scale 1); mm, cm, m, inches or feet.
  - **Opacity**, **Show**, and a width, slide, distance behind and turn for each.
    Drag a picture in the view to move it.
  - **Active**: an active picture can be clicked (that opens the panel on it) and
    dragged like a shape. An inactive one is never in the way: clicks go straight
    through it to the shapes and the plate.
  - **Show Pictures** hides or shows them all at once.
  - A picture is seen from its own side only, and fades out as the view turns edge-on.
- Pictures are kept in the `.f3d` file (as they were, so nothing is lost), with every
  change undoable, but they are never exported, sent to the slicer or added to the
  custom shape library. Files with pictures need Forge3D 0.58 or later to open
  (format 28).
- 274 unit tests (7 new: each side's picture faces its own view; sizing from two points
  keeps the first one still; clicks go through inactive and hidden pictures; a new
  picture's place; one picture per side, undo; saved and opened again, and a broken
  picture left out).

## 0.57.0 Beta — Slice 48: Split by Mask (2026-11-18)

- **Split by Mask** (Design → Change): select a shape and paint over the part to cut
  off by dragging on it (the ring shows the brush; **[** and **]** change its size;
  hold **Ctrl** to rub paint out). **Apply** cuts the shape along the edge of the paint:
  - **Both pieces** (or just the painted piece, or just the rest), named "… (masked)"
    and "… (rest)", made as one undo step.
  - Each cut is closed with a **smooth cap**: a surface stretched across the edge like
    a membrane. Both pieces get the very same cap, facing opposite ways, so they fit
    back together exactly. A band painted round a part gives two cuts, each capped.
  - The cut follows the paint's edge closely: the surface is divided finely near it
    first, so even a box's big flat faces are cut along the curve.
  - **Undo Stroke** (or Ctrl+Z) and **Clear** while painting.
- The cut is a step in the shape's history: double-click it to paint more or rub some
  out, and both pieces change together (the other piece is hidden while you do).
  **Remove the cut** gives the whole shape back. The paint moves and grows with the
  shape when it's scaled.
- Files with Split by Mask need Forge3D 0.57 or later to open (format 27).
- 267 unit tests (4 new: which dab wins where they overlap; a box's corner cut off,
  the two pieces closed and adding up to the whole box; a band round a cylinder, two
  caps; Split by Mask on the plate: both pieces, changed together, saved, scaled,
  undone).

## 0.56.0 Beta — Slice 47: Join Parts (2026-11-18)

- **Join Parts** (Design → Arrange): moves one part so it joins another. Click the
  place on the part that moves, then where it goes on the other part, and it's shown
  there straight away; Apply moves it (one Undo puts it back).
  - What a click picks is worked out from where you click:
    - a **flat face**: its middle, facing out (faces go flat together, centred);
    - a **peg** (a round side sticking out): its tip, pointing along it (on meshes too);
    - a **hole**: its bottom, pointing out of the opening (a peg goes in to the bottom),
      also in shapes that have become meshes (with a Quick Socket, sculpting, an
      import...): the round side is found from the smooth patch of surface round the
      click;
    - a **ball**: its middle, on shapes made in Forge3D and on imported or sculpted
      meshes too (a mesh is checked for a ball round the spot clicked);
    - a **round cup**, or a **Quick Socket** in the part's history: where the ball's
      middle or the peg's end goes;
    - anywhere else on a mesh: the **spot** clicked, facing out.
  - **Gap** leaves room along the join. Into a Quick Socket peg socket it starts at
    the socket's fit, so the peg's end stops short of the bottom by that much and its
    shoulder meets the surface. **Across** slides it either way, **Turn** spins it
    round the line of the join (to face it the right way or line up a peg's flats),
    and **Flip** turns it end for end if it was worked out pointing the wrong way.
  - **Pick Again** changes either pick; clicking once both are picked changes where it
    goes.
  - **Keep them together (group them)**: once joined, the two are grouped, so moving
    or turning one moves the other. Joining onto a group adds to that group (it keeps
    its name), so a figure can be built up part by part. Separate takes them apart.
- **Quick Socket: Center on the face** (All, Top-Bottom, Left-Right) moves the socket
  to the exact middle of the face it's on.
- 263 unit tests (8 new: the move itself (gap, across, turn, flip); a face against a
  face; a peg into a drilled hole, touching nothing; a ball into a Quick Socket and a
  peg seated flush in its socket; a ball found on a mesh, and just the spot on a mesh
  box; a hole found in a shape that has become a mesh, and a peg on a mesh; grouping
  when kept together, and joining onto a group; a socket centred on its face).

## 0.55.1 Beta — Features tab (2026-11-18)

- **A Features tab**, after Design, so the Design tab isn't crowded: **Holes and
  Joints** (Hole, Thread, Quick Socket), **Surface** (Shell, Follow, Edges) and
  **Repeat** (Pattern). Design keeps Combine, Arrange, Sketch, Change and Place.
- The ribbon remembers the tab you were on by its name, so new tabs don't move you
  (the first time, it works out where you were from before).
- The tour points to the Features tab for holes, threads, sockets, shells and edges.
- Added to the to-do list: Split by Mask.

## 0.55.0 Beta — Slice 46: Quick Socket (2026-11-18)

- **Quick Socket** (Design tab → Features): select a shape, click the spot where the
  opening goes, and a socket is cut in there, square to the surface, sized to fit once
  printed. It works on imported and sculpted meshes as well as shapes made in
  Forge3D, and becomes a step in the shape's history (double-click it to change it, or
  click elsewhere to move it).
  - **Ball socket**: give the ball's diameter. **Snap-in** makes the opening narrower
    than the ball by the **grip** (10% to start), so it pops in and stays; **Open**
    lets the ball drop in, its top level with the surface.
  - **Peg socket**: round, square, hexagon or octagon (flats stop it turning), with its
    size and how far it goes in. **Turn** sets which way the flats face.
  - **Printer**: Resin or FDM sets the **fit** (0.1 or 0.2 mm to start), the room left
    all round and at the bottom of a peg socket. The ball or peg keeps its true size.
  - **Relief cuts**: 2 to 8 slots from the opening down through the socket's side, so
    it can open as the ball or peg goes in. Their width and depth can be set (depth on
    Auto: to the ball's middle, or half way down a peg).
  - **Add the Matching Ball / Peg** adds the ball or peg the socket takes, at its true
    size, beside the shapes.
  - The settings used last come back next time, so a figure's joints come out alike.
  - On shapes of over 300,000 triangles, the panel shows what will be cut away in red
    instead of the finished result, so changing the settings stays quick.
- Scaling a shape evenly scales its sockets too (so they still take a ball scaled with
  it); the fit stays as it is.
- Files with sockets need Forge3D 0.55 or later to open (format 26).
- 255 unit tests (7 new: how a ball sits; a ball and a hexagon peg seated in their
  sockets without touching, and the peg no longer fitting when the socket is turned;
  round and square pegs and relief cuts; a socket on the side of a moved and turned
  shape, saved and changed; scaling; a socket in an imported mesh at a clicked spot).

## 0.54.0 Beta — Slice 45: Custom shape library (2026-11-18)

- **Save Custom Shape** (Print/Save tab → Library, and the File menu): keeps the
  selected shapes in your library under a name you give. Saving under a name that's
  already there asks before replacing it.
- **Add Shape → Custom** (and Add → Custom) lists them. One comes in **at exactly the
  size it was saved**, beside what's on the plate, so a 3 mm ball and its 3.2 mm socket
  always come in at 3 and 3.2. One Undo takes it away again.
  - Each keeps its whole history (Subtract, Smooth, Sculpt...) and colour, so it can
    still be changed after it's added.
  - Several shapes saved together come in as they sat, keeping their own names; a
    single shape takes the name it has in the library.
  - Slow results (sculpting, Smooth, big imported meshes) are saved with the shape, so
    it comes in without being worked out again.
- **Manage Custom Shapes...** (at the bottom of the Custom menu): add, rename or delete
  (to the Recycle Bin) your shapes, open their folder, or keep them in another folder,
  such as one that's backed up or synced between computers. They're kept in
  Documents\Forge3D\Custom Shapes to start with, one .f3d file per shape, so they can
  also be copied in and out by hand.
- 248 unit tests (5 new: file names; a shape back at its saved size, beside the others,
  renamed and undone; history and how shapes sit together kept; saved results come
  with the shape; rename).

## 0.53.0 Beta — Slice 44: Faster sculpting on big meshes (2026-11-18)

- **Sculpting keeps up on meshes of a million triangles and more.** Each dab of the
  brush now changes only the part of the mesh under it, and only that part is sent to
  the graphics card:
  - The points of a mesh are numbered in order through space when sculpting starts, so
    a brush's points sit close together, in memory and on the graphics card.
  - Only the blocks of points and triangles a dab changed are sent (a few thousand
    points, not the whole mesh). The card keeps a quarter more room than the mesh
    needs, so detail added under the brush is sent on its own too.
  - Finding the surface under the cursor no longer re-files every moved triangle on
    every dab; they're found on their own list meanwhile and filed when you let go
    (a share on each dab in a very long stroke).
  - Big brushes work on several processor cores at once.
- Measured with a 10 mm Draw brush on 2 processor cores: on 1.8 million triangles a
  dab takes about 2 ms (about 5 ms while adding detail) and sends about 15,000 of the
  920,000 points; on 5.2 million triangles about 6.5 ms, the slowest under 20 ms.
  Before, each dab sent the whole stretch of points between the lowest and highest
  numbered point it touched: often most of the mesh.
- Files with sculpting from 0.52 open as before; their saved results are worked out
  again once (the points are numbered differently now), so the first open after
  updating takes a little longer.
- Fixed (Linux builds with Qt 6.4 only; Windows uses a newer Qt): the app could crash
  some time after sculpting, from a fault in that Qt version's form layout. The
  Properties panel no longer uses the part that triggers it.
- 243 unit tests (1 new: every point and triangle a dab changes is among those sent,
  and a small brush sends a small part of the mesh), plus a benchmark
  (`forge_tests "[.bench]"`, not run by default).

## 0.52.0 Beta — Slice 43: Lay out pieces on plates (2026-11-17)

- **Lay Out** (Print/Save → Prepare, and the Edit menu): sets the selected shapes (all
  of them if none are selected) side by side on the build plate, ready to print, in
  rows, biggest first. When they don't all fit, the rest go onto plate 2, 3... drawn
  beside plate 1. Two ways:
  - **Flattest Side Down** turns each shape onto the side with the most of it touching
    the plate first (it keeps the way it stands unless another side is clearly better).
  - **As They Stand** only moves them.
  Shapes turn a quarter on the plate when that fits better. Anything too big for the
  printer is left where it is, and the status bar says so. One Undo puts everything
  back.
- **Split to Fit → "Then lay the pieces out on plates"** (on by default, remembered):
  after cutting, each piece is laid on a cut face where it can be (pegs up, holes down,
  so they print round), or else its flattest side, and set out on the plates. Cutting
  and laying out are one undo step.
- **Plates 2, 3...** are drawn while something sits on them. A shape wholly on any
  plate isn't counted as off the plate.
- **Export** of shapes on more than one plate asks: **One File per Plate** ("Model -
  plate 1.3mf", "Model - plate 2.3mf"..., each plate's shapes where they'd sit on the
  printer) or **One File**.
- Fixed: a step made of several parts that failed half way (Split, Split to Fit,
  Duplicate, Import...) could undo the step before it as well when it tidied up. It
  now undoes only what it had done itself.
- 242 unit tests (4 new: flattest side; split pieces never on their pegs, standing on
  the cut when it's favoured; several plates with nothing too big moved; a failed step
  undone without touching the one before).

## 0.51.0 Beta — Slice 42: Split to Fit shows its cuts first; angled cuts (2026-11-17)

- **Split to Fit shows its cuts before making them.** Instead of a question box, a
  panel lists the planned cuts, and each one is drawn through the model in the view
  (the picked one yellow, with an arrow; the others blue).
  - **Move a cut**: drag its plane in the view, or pick it in the list and type its
    **Position**. **Lean** tilts it (up to 45°), **Lean around** says which way.
  - The panel says straight away whether every piece will still fit the printer, or
    which piece could be too long, which cuts cross, or which cut misses the model.
    Split still works if you want it anyway (it asks first).
  - **Let cuts lean (angled cuts)** plans again, letting each cut lean up to 30°
    where that cuts through less (along a leaning arm or neck, say). It's remembered.
    **Plan Again** forgets your changes.
  - **Split** makes the cuts as one undo step; **Cancel** or Esc leaves the model as it
    was.
- Cuts now go right through the model (each one splits every piece it passes through),
  so what you see is what you get. Before, each piece was cut on its own.
- **Pegs keep clear of the other cuts**, so a later cut no longer slices through the
  pegs and holes of an earlier one.
- Fixed: the arrow on the Split plane counted as a shape off the plate in the status
  bar.
- 238 unit tests (a new Split to Fit test covers planning, leaning, checking, moving,
  making, a grid of cuts and pegs kept clear of other cuts).

## 0.50.0 Beta — Slice 41: Open PLY and glTF / GLB (2026-11-17)

- **Import PLY** (what most 3D scanners and scanning apps save): text or binary, with
  any of the usual kinds of numbers. Faces with more than three corners are split into
  triangles. A scan coloured point by point comes in with its average colour. A PLY
  that's only points (a point cloud, no surface) is refused with a message saying to
  turn it into a mesh first. PLY has no units, so a tiny import offers to scale it, as
  STL does.
- **Import glTF and GLB** (models from the web, Blender, game tools): each part of the
  model comes in as its own shape, named as in the file, placed as the file places it,
  turned upright (glTF's "up" is Y, Forge3D's is Z) and converted from metres to
  millimetres, with no question about scale. A material's base colour becomes the
  shape's colour. A `.gltf` can keep its data inside it or in a `.bin` file beside it.
  Compressed (Draco / meshopt) and old version 1 files get a message saying to export
  them again.
- Import (Ctrl+I) and dragging files onto the window both take `.ply`, `.glb` and `.gltf`.
- Damaged PLY and glTF files are refused with a plain message (cut short, numbers that
  aren't numbers, points that don't exist, data running past the end).
- 237 unit tests (3 new: PLY text and binary, both byte orders; glTF / GLB embedded,
  separate and mirrored; a GLB import in millimetres with its colour).

## 0.49.0 Beta — Slice 40: Smooth, Extrude Down, Solid / See-Through / Wireframe (2026-11-17)

- **View → Look** (and the View tab): **Solid**, **See-Through** or **Wireframe**,
  remembered between sessions.
  - **Fixed:** shapes were drawn with their far faces in front of their near ones
    after the first moment, so they looked hollow, as if you could see into them. The
    view-cube and label drawing switched off depth testing, and it wasn't switched back
    on. **Solid** now draws shapes as they will print.
  - **See-Through** draws every shape half clear (far side first, then the near side),
    so you can see into it and what's behind it.
  - **Wireframe** draws the edges of every triangle.
- **Smooth** (Sculpt tab → Whole Shape): rounds off the whole surface of the selected
  shape, edges and corners included, keeping its volume. The triangles are evened out
  again every few rounds, so strong smoothing stays even (no lumps along the edges). It's a step: set **Strength**
  (1 takes the edge off, 10 rounds the shape right off) in Properties, or remove it
  with **Remove the smoothing**. Plain shapes are first evened out into small triangles.
- **Extrude Down** (Design tab → Place): stretches the underside of the selected shape
  straight down to the plate, filling everything below it (under a ball, a ledge, a
  figure's arms), so it prints without supports. It's a step too (**Remove the
  extrusion**); move the shape afterwards and the added part moves with it.
- On very big meshes (over a million triangles) Smooth and Extrude Down take several
  seconds.
- **Opening files is quick again with slow steps in them.** Saving now also keeps the
  finished triangles of every shape the mesh engine works out (Smooth, Extrude Down,
  sculpting, merges of imported meshes...), with a fingerprint of what they were made
  from. Opening the file uses them instead of working everything out again, unless the
  shape was changed since. Files get bigger by about the size of those shapes.
  Listing a shape's steps in Objects no longer works them all out either.
- File format 25 (for the two new steps): Forge3D 0.48 and older can't open files saved
  by this version.
- 234 unit tests (4 new: Smooth, and an even result at strength 10; Extrude Down on a
  lifted box, a ball, a T and a ring; results saved with the file and used on opening).

## 0.48.0 Beta — Slice 39: Colours (2026-11-17)

- **Colour** (Build tab, and right-click a shape in Objects): pick one of 14 filament
  colours (white, greys, black, red, orange, yellow, green, teal, blue, purple, pink,
  brown, gold), **More Colours…** for any colour, or **Automatic** to go back to the
  colour Forge3D gave it. It colours every selected object (a part picked inside a
  combined shape colours the whole object). One Undo puts the old colours back.
- Colours are **saved with your model**, and **written to 3MF** files (one standard 3MF
  material per colour, each object pointing at its own), for slicers and printers that
  print in more than one colour. 3MF files with colours keep them when imported.
- A combined shape, and every step on a shape (edges, holes, split, emboss…), now
  shows the colour of its first part instead of a new colour each time, until you
  give it one of its own.
- Older versions of Forge3D open these files too; they just don't show the colours.
- 230 unit tests (2 new: colours chosen, inherited, undone and saved; colours through
  3MF and back).

## 0.47.1 Beta — Code audit: damaged files and edge cases (2026-11-17)

No new features: a sweep for crashes, hangs and wrong behaviour at the edges.

- **Damaged project files can't crash Forge3D.** A sketch in a file that refers to a
  point, curve or radius that isn't there (or a rule on the wrong kind of curve) now
  stops the file opening with "The file is damaged…", where before it could crash.
  Numbers no Forge3D file would hold (over 10 km, or not a number) are refused the
  same way, where before they could make Forge3D work for ever.
- **Damaged STL / OBJ / 3MF files** give a plain message instead of a technical one
  (words where numbers should be, impossible point numbers, points that are not a
  number or absurdly far away), and a 3MF whose parts hold copies of copies past
  30 million triangles is refused instead of filling the memory.
- **Sizes that can't work are refused at once** instead of leaving Forge3D busy:
  shape sizes under 0.0001 mm or over 1 km; an edge rounding or chamfer bigger than
  the whole shape; a thread with more than 2000 turns (too fine a pitch for its
  length); more than 120 letters in one piece of text (the text boxes stop typing a
  little past that).
- **One shape that can't be built no longer hides the others**: it is left out of the
  view, the status bar names it, and every other shape still shows.
- **A last safety net**: an unexpected error in any step now shows "Something went
  wrong… Forge3D is still running" (with advice to save under a new name) instead of
  closing the app.
- **Backups**: deleting everything and leaving it now removes the backup copy, so an
  old version isn't offered back after a crash.
- Tests: a random Undo / Redo test (every state must match exactly), a damaged-file
  test for sketches and imports, and an optional file fuzzer (`[.fuzz]`) that changes
  every kind of value in saved documents. 228 unit tests.

## 0.47.0 Beta — Slice 38: Split for printing (2026-11-16)

- **Alignment pegs on Split** (Design → Split, keeping both halves): the cut is keyed so
  the pieces only go together one way when you glue them. On by default.
  - **Pegs on one piece**: tapered pegs stand out of one piece (with a level cut, the
    lower one, so both pieces still print the usual way up) into matching tapered
    holes in the other. The taper makes a peg easy to start in its hole.
  - **Loose pins**: holes in both pieces, and pins (tapered both ends) added beside
    the model to print separately, so every piece keeps a flat cut face to print on.
  - **Diameter** (5 mm), **length** (6 mm), **taper** (2°) and **clearance** (0.2 mm,
    added to the hole) can be changed, and are remembered.
  - Pegs go well inside the cut, clear of its edges and of any holes through it, with
    room behind them for the hole: two per piece of the cut (so it can't turn), three
    on a big one. A cut through two legs gets pegs in each leg. Where the cut has no
    room for a peg, smaller ones are tried; if none fits, a warning says the cut has no
    room for pegs, how much it would need, and offers **Undo the Split**.
- **Split to Fit** (Print/Save tab, Edit menu, and a **Smart Split** button in Split):
  cuts a shape too big for your printer (Build Plate Size) into pieces that each fit,
  as few as will do. Each cut is level or upright and goes where it cuts through
  least (through a thin neck rather than a thick end), and every cut is keyed. It
  says how many pieces first; one Undo puts it back.
- The fit warning now tells **too big** (orange-red: Split to Fit will help) from **off
  the plate** (amber: it would fit if moved onto the plate). The split pieces stay
  together, amber, so you can see how they fit; move one onto the plate to print it.
- 225 unit tests (3 new: keyed cuts, pins and small cuts, the smart split).

## 0.46.0 Beta — Slice 37: Dark theme (2026-11-16)

- **View → Theme**: **Follow Windows** (the first time: matches Windows' own light or
  dark app mode), **Light** or **Dark**. It switches straight away, no restart, and is
  remembered.
- In Dark, everything goes dark together: the menus, ribbon and panels, the 3D view and
  its grid, the sketcher, the view cube, measurement labels and the tour's notes. The
  ribbon's icons turn light.
- (Shapes keep their own colours either way.)

## 0.45.0 Beta — Slice 36: Sketch on the section's cut (2026-11-16)

- With **Section View** on, you can sketch on the flat cut it shows:
  - Select a shape, click **Sketch**, then click the **cut face** itself; or
  - **Sketch ▾ → The section view's cut** (with a shape selected, or with nothing
    selected for a sketch at that plane on the plate).
- The sketch opens on the cut's plane, exactly where the slider is, with the cut
  through the shape drawn in grey dashes to snap to: its walls, holes and hollows.
- What you make goes **into the part that shows** (The other way is set for you), so a
  **Pocket** cuts into the half you can see, from the inside: a hidden chamber, a
  channel, a slot from the middle. **Both ways** cuts or pads to both sides of the
  cut, and **Revolve** / **Groove** work as on the middle planes.
- The result is an ordinary step: double-click it to change the sketch. It stays where
  it was made if the section slider moves afterwards.

## 0.44.0 Beta — Slice 35: Backup settings, section clicks, measurement labels (2026-11-16)

- **Edit → Backup Copies…**: how often a backup copy of unsaved work is made, from
  **2 to 60 minutes**, or **off** altogether (unticking "Keep a backup copy of unsaved
  work"). Turning it off also removes this session's backup.
- **Section View and clicks**: with the view cut open, a click no longer picks a shape
  through the part that's hidden. It lands on what you can see: the cut face, the
  inside, or the shape behind. (Selecting, dragging, Lay on Face and Measure.)
- **Measure shows its numbers in the view**: the distance between the two things you
  picked is written on its green line (and centre to centre on the blue one), as well
  as in the panel.
- **Keep on Screen** (Measure panel): leaves that measurement showing and clears the
  picks, so you can measure something else and see several at once. Kept ones stay
  after Done. **Remove Kept** takes them away; they're also taken away when the model
  changes, since the numbers might no longer be true.
- The Quick Start guide's pictures show the new ribbon, and it mentions Orbit / Pan / Zoom
  on the View tab.
- 222 unit tests (1 new: picking with a section).

## 0.43.0 Beta — Slice 34: Backup copies and recovery (2026-11-16)

- While you work, Forge3D keeps a **backup copy** of a model with unsaved changes,
  refreshed every 2 minutes (only when something changed, and never in the middle of
  a drag). It's kept in Forge3D's own data folder, not beside your file, and your file
  is never touched.
- If Forge3D **didn't close properly** (a crash, a power cut, the computer restarting),
  the next start offers: **Recover** (opens the backup with its changes not yet saved,
  under the model's own name, so Save puts it back where it belongs), **Throw It
  Away**, or **Ask Me Next Time**.
- Saving, opening another model, starting a new one or closing normally removes the
  backup: there's nothing to recover then.
- Two Forge3D windows open at once each keep their own backup and don't offer each
  other's.

## 0.42.0 Beta — Slice 33: Does it fit the printer? (2026-11-16)

- **Build Plate Size** also has the **height** the printer can print (Z). Choosing a
  bed from the list fills in a usual height for it; check your printer's and change it
  if it differs. A faint box over the plate shows the space.
- Shapes that **don't fit** (reaching past the plate's edge, or above the height) are
  drawn **orange-red**, and the status bar says how many there are. Move, turn or
  shrink a shape and it goes back to its own colour as soon as it fits.
  - A shape turned at an angle is checked by its real outline, not its box.
  - Shapes sunk below the plate aren't flagged (slicers cut those off).
  - **Show shapes that don't fit in orange-red** in Build Plate Size turns it off.

## 0.41.0 Beta — Slice 32: Recent files, plate size, the ribbon (2026-11-16)

- **File → Open Recent**: the last six models opened or saved, newest first (press
  its number to open one). A file that has been moved or deleted is taken off the
  list when you pick it. **Clear This List** empties it.
- **Build Plate Size** (Edit menu, View menu, and the View tab): set the grid to your
  printer's bed, from a list of common sizes (180 × 180 up to 360 × 360 mm) or any
  size typed in. The plate now has a darker line round its edge, so it's easy to see
  whether a design fits. Remembered for every model.
- **The ribbon, reorganised**, with a new **View** tab:
  - **Build**: File, Add, Handles, Remove.
  - **Design**: Combine and Arrange (moved from Build), Sketch, Features, Change, and
    Place (Lay on Face / Sit on Plate, moved from Print).
  - **Sculpt**: Brushes, Triangles.
  - **View**: **Workspace** (Orbit, Pan, Zoom, Fit, Reset View), **Objects** (Section
    View) and **Plate** (Plate Size).
  - **Print/Save** (was Print): Save, Export, Send to Slicer.
  - **Check** (Measure) and **Undo** are on every tab.
- **Orbit / Pan / Zoom** (View tab): click one and dragging with the **left** button
  turns, slides or zooms the view, for a touchpad or a mouse without a middle button.
  It stays on (the button shows pressed) until you click it again; the right button,
  middle button and wheel keep working as always.
- The tour and the Quick Start guide's words follow the new tabs. (The guide's
  pictures still show the old ribbon.)

## 0.40.1 Beta — Section view for one shape (2026-11-16)

- Section View has a **Cut:** choice: **Everything on the plate** (as before) or
  **Only the selected shape**. With the second, just the shape you select is cut open
  and the others stay whole; click another shape to cut that one instead. With
  nothing selected, the last one stays cut. The slider then travels across that
  shape only.
- The Section View panel opens tall enough to show all of itself.

## 0.40.0 Beta — Slice 31: Section view (2026-11-16)

- **View → Section View**: cuts the view open so you can look inside a model: wall
  thicknesses, holes that don't go through, how parts sit inside each other. A small
  **Section View** panel opens under Objects:
  - **Cut across**: left to right (X), front to back (Y) or bottom to top (Z).
  - **Where**: a slider through the model, an exact position in mm, and **Middle of
    the model**.
  - **Show the other side** swaps which part is hidden.
- Cut faces are drawn in a flat, darker shade of the shape's colour.
- Only the view is cut. The model stays whole: saving, exporting and printing aren't
  affected, and every tool keeps working while the view is cut open. Close the panel
  (or untick View → Section View) to see everything again.
- Good to know: a click can still pick a shape through the part that's hidden.

## 0.39.0 Beta — Slice 30: Text in sketches (2026-11-16)

- **Text** tool in the sketcher (W): click where the words start, then type them and
  choose the **font**, bold / italic, the **letter height** (of a capital, in mm) and
  how far they're turned. The letters are drawn as outlines and are part of the
  sketch:
  - **Pad** a sketch of words: letters standing up. **Pocket** them on a face:
    engraved, with crisp flat-bottomed letters (and Taper works on them).
  - Words **inside an outline** are cut out of it, like a stencil or a nameplate with
    see-through letters.
- **Double-click** words to change them; drag them to move them; pick them and press
  Delete to remove them. The small square at their start is an ordinary sketch point,
  so dimensions (Across, Up) and rules can hold the words in place.
- Any font installed on the computer. A file opened on a computer without the font
  uses the default font instead.
- (For words that follow a curved surface, Emboss is still the tool; sketch words are
  flat.)
- 221 unit tests (2 new).

## 0.38.0 Beta — Slice 29: Taper (2026-11-16)

- **Taper** on pads and pockets (in the sketcher, under Length / Depth): an angle that
  leans the sides in as they go away from the sketch, the "draft" that moulded parts
  have. 0° is straight as before; a minus angle leans them out. Up to 60° either way.
  - A pad is full size at the sketch and narrower at its far end; a pocket gets
    narrower as it goes deeper. Round outlines become cones.
  - A hole inside the outline opens out as the outside narrows (the walls thin from
    both sides).
  - With **Both ways** it narrows each way from the sketch. **Through all** pockets
    stay straight.
  - If the angle is too steep for the length (the sides would meet before the end),
    a message says so.
- Change it later by double-clicking the pad or pocket. Made taller or shorter by its
  handles, a tapered shape keeps its shape (the angle follows); it resizes evenly
  across.
- (Chamfered edges were already there: Design → Edges → Chamfer or Bevel.)
- 219 unit tests (2 new).

## 0.37.0 Beta — Slice 28: Sketch tools (2026-11-16)

Four new tools on the sketcher's toolbar:

- **Trim** (X): click a piece of a line, arc or circle to take it away, back to where
  other lines cross it. The piece under the mouse shows red first. A line crossed
  twice splits in two (the halves stay in line); a circle becomes an arc; a line
  nothing crosses goes altogether. The new ends stay on the line that cut them.
- **Round corner** (Shift+R): pick corner points where two straight lines meet, give a
  radius, and each corner becomes an arc touching both lines. The radius is a
  dimension (double-click to change it). The old corner stays as a dashed helper
  corner, so sizes measured to it still hold.
- **Offset** (Shift+O): pick any line or curve of an outline and give a distance: the
  whole outline is copied that far out (a minus number: in). The copy is held that
  far from the original by **Gap** dimensions (double-click one to change it), so it
  follows when the original is dragged or resized.
- **Mirror** (M): pick things, Ctrl+click the line to mirror about last (or include one
  dashed helper line), and they're copied to the other side. The copy stays a mirror
  image when the original changes; points on the line are shared, so half a shape
  drawn against the line closes up. A mirror line that leans a little (up to 10°) is
  made upright or level first, and ends drawn on it (or all but) are put on it.
- Each tool is one Undo, and says why when it can't be done (a radius too big for the
  lines, an offset that would fold over itself).
- Files saved with 0.37 need 0.37 or later (file format 24: the Gap dimension).
- 217 unit tests (4 new: one for each tool).

## 0.36.0 Beta — Slice 27: Loft and sweep (2026-11-16)

- **Loft** (in the sketcher, **Make:**): blends one outline into others stacked above
  it, such as a square base to a round top, a vase or a funnel. Draw the first outline,
  click **Add one above** and change the copy into the next one at its height; the
  outlines either side show in grey. **Below** / **Above** move between them, **Remove
  this outline** takes one out, and **Straight sides** gives flat faces between them
  instead of a smooth blend.
- **Sweep** (in the sketcher, **Make:**): moves an outline along a path.
  - **Along: a path you draw**: draw the outline, click **Draw the path ▶** and draw
    the path standing up from it (lines and arcs, such as a pipe, a handle or a bent
    rod). Corners are rounded off.
  - **Along: a spiral**: a spring or coil. Draw the outline to one side of the up-down
    line; set **Pitch** (rise per turn), **Turns** and **left-hand**.
- Both are shapes in the history: double-click to change the outlines, heights, path
  or spiral. They resize evenly (keep proportions), and add, subtract and round off
  like other shapes.
- Files saved with 0.36 need 0.36 or later (file format 23).
- 213 unit tests (3 new: lofts, paths and springs, saving and resizing).

## 0.35.0 Beta — Slice 26: Threads (2026-11-16)

- **Thread** (Design tab): cuts a screw thread into a round face of a shape. Click the
  outside of a **peg** for a bolt-like outside thread, or the inside of a **hole** for a
  nut-like inside thread.
- **Thread size**: standard metric sizes M2 to M30 (ISO coarse pitches), suggested
  from the face you click (a hole: the size it was drilled for), or any diameter and
  pitch under **Other size**.
- **Fit** (0.2 mm to start) loosens it for printing: a peg's thread is made that much
  smaller all round, a hole's that much bigger, so printed parts screw together.
  **Left-hand** threads too.
- A preview before **Apply**; it's a step in the history (double-click to change), the
  thread follows when the peg or hole changes size, and it grows with the shape when
  it's resized evenly. It runs the length of the round face and stops at its ends, so
  it doesn't cut into what the peg stands on or the bottom of a hole.
- Threads are made with the mesh engine (like Emboss), so add them after rounding and
  other solid steps.
- Files saved with 0.35 need 0.35 or later (file format 22).
- 210 unit tests (2 new: sizes and round faces; an M6 peg and an M6 hole, left-hand
  threads, saving, a longer peg, resizing).

## 0.34.0 Beta — Slice 25: Measure (2026-11-14)

- **Measure** (on the Build, Design and Print tabs): click a **face**, an **edge** or a
  **corner** of a shape (hold **Shift** for an exact point; on imported meshes, points
  only). What's under the mouse lights up orange; what's picked turns red.
- One thing picked: a flat face's **area**; a round face's or edge's **diameter**
  (holes, pegs, cylinders); a straight edge's **length**; cones, balls and rings too;
  a point's position.
- Two things: the **distance** where they come nearest (drawn in green, with how far
  along X, Y and Z), the **angle** between them, whether they're **parallel** and how
  far **apart**, **centre to centre** of two holes, pegs or circles, and a hole's
  **centre to a face** (drawn in blue).
- A third click starts again; **Clear** too. Numbers can be selected and copied.
- 208 unit tests (2 new: faces and edges described; distances, angles and centres
  between them).

## 0.33.0 Beta — Slice 24: Pattern brush (2026-11-14)

- **Pattern** brush: presses a repeating pattern into the surface where you drag:
  **Scales**, **Hexagons**, **Woven cloth**, **Skin** or **Bricks**, or **Your own
  picture...** (light parts stand highest; it repeats side by side). **Pattern size**
  sets how wide one repeat is, Strength how deep it goes; Ctrl engraves it instead.
- Going over the same place again doesn't press it any deeper, so strokes can overlap
  freely; strokes that meet line up, and Mirror works (the other side mirrored).
- Detail is added under the brush finely enough for the pattern.
- Kept in the Sculpted step like any stroke (your own pictures are saved in the file),
  played again when the shape changes, and grows with the shape when it's resized.
- Files saved with 0.33 need 0.33 or later (file format 21).
- 206 unit tests (1 new: the built-in patterns repeat seamlessly; pressed in to their
  depth and no deeper; engraved; kept, played back and saved, with a picture of one's
  own).

## 0.32.0 Beta — Slice 23: Remesh and Simplify (2026-11-14)

- **Remesh** (in the Sculpt panel): makes the whole shape of even triangles of the
  **Triangle size** you set. Tidies up areas stretched by Snake hook or Grab, and thins
  out over-detailed ones. Kept as a step in the strokes, so it's done again when the
  shape under it changes; a mask comes along with it; Undo stroke takes it back.
- **Simplify** (Sculpt tab): fewer triangles for any big shape (imported scans and
  models, or sculpted shapes), keeping its look. It's a step: set **Keep: % of the
  triangles** in Properties (half to start); Properties shows the counts before and
  after. **Remove the simplifying** gives the full shape back. Closed shapes stay
  closed; flat areas lose triangles first, so edges stay sharp.
- Fixed: sculpting disappeared when the shape under it grew or shrank (a sphere made
  bigger): each stroke now starts from the nearest point of the surface as it is, so it
  stays on it.
- Fixed: Crease, Snake hook and mask strokes were read back from a file as Grab
  (0.31 was never published).
- Files saved with 0.32 need 0.32 or later (file format 20).
- 205 unit tests (6 new: remeshing evenly and keeping a mask; simplifying a sphere, a
  box and a raw tessellation; Simplify steps; every brush saved and read back; strokes
  on a sphere made bigger and smaller).

## 0.31.0 Beta — Slice 22: Mask, Crease and Snake hook (2026-11-14)

- **Mask** brush: paint over parts you want to protect (they turn dark); the other
  brushes leave them alone, or touch them less where the mask is light. Ctrl unpaints.
  **Clear mask** and **Invert mask** buttons work on the whole shape.
- **Crease** brush: cuts a sharp, narrow groove where you drag, for folds, seams and
  wrinkles. Ctrl makes a sharp ridge.
- **Snake hook** brush: pulls the surface out after the mouse, for horns, tails and
  tentacles; detail is added as it stretches, and it works with Mirror.
- All three are kept in the Sculpted step like the other strokes: the mask comes back
  when you double-click the step to add more, and Undo stroke takes back mask changes.
- Files saved with 0.31 need 0.31 or later (file format 19).
- 199 unit tests (1 new: Crease, Snake hook, and the mask painting, inverting and
  clearing, live and played back).

## 0.30.0 Beta — Slice 21: Sculpting kept in the history (2026-11-13)

- **Sculpting is now a step** in the shape's history, like Emboss or Shell, instead of
  turning the shape into a fixed mesh. Done adds a **Sculpted** step showing how many
  strokes it has.
- The strokes are kept, not just the result: **change a size, a hole or anything below
  the step and the strokes are made again** on the new shape, where they were.
- **Double-click the step** (or right-click → Change...) to add more strokes to it.
  Undo stroke takes back only the new ones; Esc leaves the step as it was.
- **Remove the sculpting** in Properties gives back the shape as it was before.
- The strokes stay on the part: **move or turn the shape inside the step** and they go
  with it (moving it is instant).
- Sculpting a shape that is rolled back puts the step in at the rollback bar.
- Saved in the file (format 18), and grows with the shape when it is resized evenly.
- 198 unit tests (2 new: strokes played again give exactly the live result, follow a
  longer box, survive saving and resizing; mirrored strokes stay at the corners when
  the box inside is moved, turned and the whole resized).

## 0.29.0 Beta — Slice 20: View cube (2026-11-13)

- A **view cube** in the top-right corner of the 3D view, like FreeCAD's: it turns with
  the view, its sides labelled Front, Back, Left, Right, Top and Bottom, so you can
  always see which way you're looking.
  - Click a **side** to look straight at it, an **edge** to look from between two sides,
    a **corner** for a three-quarter view. The view turns smoothly there.
  - Under it: **turn 90° left**, **home** (back to the starting view) and **turn 90°
    right**.
  - The piece under the mouse lights up blue.
- **View → View Cube** shows or hides it (remembered).

## 0.28.0 Beta — Slice 19: Detail where you sculpt (2026-11-13)

- **Add detail where I sculpt** (on by default): as a brush goes over the shape, the
  triangles under it are split finer, so strokes stay crisp without making the whole
  shape heavy. **Finest detail** (mm) sets how small they get; a big brush adds coarser
  ones (a twelfth of its size), a small one finer.
- A new starting **Detail: Low (about 40,000 triangles)**, for building detail only
  where you sculpt.
- **Finding the spot under the mouse is about 2,000 times faster** on big meshes (a grid
  of the triangles instead of testing every one): the brush ring keeps up even at High.
- Undo stroke takes back added detail too.
- 196 unit tests (1 new: detail added under the brush only, the surface still closed,
  and the grid finding the right spot).

## 0.27.0 Beta — Slice 18: More brushes and symmetry (2026-11-13)

- Five new sculpting brushes (Brush, with a line under it saying what each does):
  - **Inflate**: swells the surface out like a balloon, each spot along its own facing
    (Ctrl: shrinks).
  - **Flatten**: presses an area flat.
  - **Pinch**: draws the surface together under the brush, for sharp creases and edges
    (Ctrl: pushes apart).
  - **Clay**: builds up in flat layers, like strips of clay (Ctrl: digs out).
  - **Grab**: pulls a whole area along with the mouse, like soft toffee; it moves across
    a plane facing you, so turn the view to pull another way.
- **Mirror** X (left-right), Y (front-back) and Z (up-down), across the middle of the
  shape: whatever a brush does on one side happens on the other too, Grab included. Tick
  more than one for 4 or 8 copies.
- Shift still smooths with any brush, and Ctrl gives the opposite (Draw, Inflate,
  Pinch, Clay).
- 195 unit tests (2 new: each new brush; mirrored dabs and a mirrored Grab).

## 0.26.0 Beta — Slice 17: Sculpting prototype (2026-11-13)

The first step of Phase 2, to try out how sculpting feels and how fast it is.

- **Sculpt tab → Sculpt**: select one shape, click Sculpt, and push and pull its surface
  like clay. The shape is divided into fine triangles first (**Detail**: Medium, about
  150,000, or High, about 500,000).
  - **Draw** pushes the surface out where you drag; hold **Ctrl** to push in.
  - **Smooth** evens it out; hold **Shift** with Draw to smooth without switching.
  - **Size** (the brush's radius, also **[** and **]**) and **Strength**. An orange
    ring shows the brush on the surface.
  - **Undo stroke** (or Ctrl+Z) takes back the last strokes; **Done** keeps the result;
    **Cancel** (Esc) leaves the shape as it was.
- Prototype limits: the sculpted shape becomes a mesh (like an imported one), so its
  earlier steps can't be changed afterwards (Undo after Done puts everything back).
  No symmetry, masking or other brushes yet.
- Only the part of the shape the brush moves is sent to the graphics card again, so
  strokes stay quick on big meshes.
- Mesh shapes now say "A mesh: N triangles" in Properties (was "Imported").
- 193 unit tests (3 new: welding and splitting keep a shape closed; Draw, inverted Draw
  and Smooth; a dab on 800,000 triangles in a few milliseconds).

## 0.25.0 Beta — Slice 16: the first public release (2026-11-13)

- **An installer**: `Forge3D-0.25.0-Setup.exe` installs Forge3D just for you (no
  administrator needed) or for everyone on the PC, adds it to the Start menu (and the
  desktop, if you tick it), lets `.f3d` files open in Forge3D with a double-click, and
  can be removed from Windows Settings → Apps. A **portable zip** runs without
  installing.
- **Releases on GitHub**: pushing a version tag builds and publishes both on the
  Releases page, with release notes (see docs/RELEASING.md).
- **Forge3D's own icon**: a blue tile with a cube, on the program, the window, the
  taskbar and `.f3d` files.
- **Check for Updates** (Help menu). The first time Forge3D starts (after the tour) it
  asks whether to check at each start; **Help → Check for Updates at Start** changes
  that. It only asks GitHub for the list of versions. A newer one shows as a link in the
  status bar (at start) or a message with a Download button (from the menu).
- The version reads "0.25.0 Beta" in the title bar and About.
- 190 unit tests (2 new: comparing versions; finding the newest release in GitHub's
  list, skipping drafts).

## 0.24.1 — Settings are remembered (2026-11-13)

- The tour came back every time Forge3D started, even after finishing it. Some
  preferences were being kept somewhere Windows doesn't allow without an organisation
  name, so they were quietly never saved. Now everything is kept with the rest of
  Forge3D's preferences (HKEY_CURRENT_USER\Software\Forge3D). This also fixes Forge3D
  forgetting:
  - which slicer Send to Slicer uses (it asked again each time Forge3D started),
  - the folder and file type you last exported to, and the folder you last imported from,
  - which ribbon tab was open.

## 0.24.0 — Slice 15: Quick start (2026-11-13)

- **Help → Quick Start Guide**: an illustrated walk-through of a first print (a key tag
  with a hole and your name on it, about ten minutes), in its own window beside
  Forge3D so you can follow along. The same guide is on GitHub as
  [docs/QUICK_START.md](docs/QUICK_START.md).
- **A guided tour** the first time Forge3D starts (and any time from **Help → Take the
  Tour**): nine short notes, each pointing at the part of the window it's about (Add
  Shape, Properties, the handles, Subtract, Objects, Emboss, Send to Slicer), with
  Back / Next / Skip. The window keeps working while it's open.
- **Properties scrolls** when it's taller than the window (a big shape's settings, or
  Emboss with a picture), instead of making the window taller than a laptop screen.
  Fixes Forge3D opening taller than the screen on 768- and 900-pixel-high displays.

## 0.23.0 — Slice 14: Icons (2026-11-13)

- Every ribbon button has a picture now, with its name underneath (like 3D Builder):
  clean dark-grey line drawings with one blue accent, 32 px. Greyed-out buttons show
  greyed pictures.
- The same pictures sit beside the commands in the menus, and **Add Shape** shows a
  little picture of each shape (box, cylinder, sphere, cone, torus, tube, wedge,
  pyramid, text).
- On a dark Windows theme the lines turn light so they stay easy to see.
- The pictures are drawn from small SVG files built into Forge3D (nothing extra to
  install).

## 0.22.0 — Slice 13: Phase 1 polish (2026-11-13)

- **Lay on Face** (Print tab → Place): click it, then click any face of a shape; the
  shape turns the least it needs to so that face lies flat on the plate (the way you'd
  print it), staying where it is across the plate. One undo step. The handles hide while
  you pick; Esc cancels.
- **Sit on Plate** is now in the ribbon too (Print → Place), and works on several
  selected shapes at once.
- **A first step on an empty plate**: a note over the view says how to start (Add
  Shape, the Add menu, Import, or drop a file); it goes once there's something on the
  plate.
- **The ribbon fits narrower windows**: rather than squashing the button names, a row
  that doesn't fit scrolls sideways (a scroll bar appears under it). Undo / Redo keep
  their short names in the ribbon (what they'd undo shows when you point at them), so
  the ribbon doesn't change width as you work. The window starts no bigger than the
  screen.
- **Double-click in Objects** opens a step every time, even when its row wasn't
  selected yet (the first click used to rebuild the list and lose the double-click).
  Double-clicking a row with nothing to open (a Merge, say) opens or closes it instead.
- 188 unit tests (1 new: Lay on Face onto a box's end, its top, and a wedge's slope).

## 0.21.0 — Slice 12: Emboss a picture (2026-11-13)

- **Emboss** now takes a picture as well as words: choose **Emboss: A picture (logo or
  photo)** at the top of the panel, then **Choose a picture...** (PNG, JPG, BMP, GIF or
  SVG).
  - **Logo (one height)**: the dark parts are traced to a clean, smooth outline (holes
    kept) and raised or engraved all at one depth. **Where dark ends** (a slider) sets
    which greys count; **Invert** makes the light parts the shape instead.
  - **Photo (shades become heights)**: every shade becomes a height, darkest standing
    out the full depth and white not at all (a carved / lithophane look). **Invert**
    makes the light parts stand highest.
  - **Width** (the height follows the picture's shape), **Depth**, **Turn**, Raised or
    Engraved, and **Center on the face**, as for words. Live preview.
- Like words, a picture follows the surface: flat faces, round cylinders and spheres,
  and over edges onto the next face. Too wide to go round the shape is refused with a
  message.
- It's a step in the history: double-click it (or **Change the picture...** in
  Properties) to change the picture or its settings, or click to move it. Resizing the
  shape evenly grows the picture with it.
- The picture is kept inside the `.f3d` file (as grey, at most 1000 pixels on its longest
  side), so the file opens anywhere. File format version 17. Older files still open.
- Emboss (words too) now notices when something would wrap all the way round a shape and
  run into itself, instead of making a broken shape.
- 187 unit tests (5 new: tracing logos, holes and inverting; photo reliefs; pictures on
  a box and round a cylinder, raised and engraved; too wide to go round; saving, opening
  and resizing).

## 0.20.2 — Long Follow grooves cut cleanly (2026-11-13)

- Changing a Follow groove's path to a longer line round a cylinder could make it look
  like a ridge, or cut nothing at all, even with Groove chosen. Two causes, both fixed:
  - for some lengths the cut quietly removed nothing; it now uses a small tolerance and
    cuts every time;
  - a path that goes past all the way round (or doubles back) runs the groove through
    itself, which can't be made into a sound shape. It's now refused with "The path
    runs back over itself…" (shown in the Follow panel, Apply greyed) instead of giving
    a broken shape.
- A groove that removes nothing, a ridge that adds nothing, or a result with a hollow
  inside is now reported as a failure instead of being kept.
- 182 unit tests (1 new: grooves and ridges 40–140 mm round a cylinder).

## 0.20.1 — Overlapping pattern copies no longer hang (2026-11-13)

- Repeating a Follow groove round a circle froze Forge3D for about two minutes and then
  gave a broken shape, when the copies overlapped or nearly touched. The copies are now
  joined with a small tolerance and in parallel, so the same pattern takes a second or
  two and comes out sound.
- Safety nets: if making the copies takes more than 30 seconds it stops with "Making
  the copies took too long…"; if the result isn't a sound shape the step is refused
  with "The copies overlap in a way that can't be made into a sound shape…". In the
  Pattern panel either shows as the message (Apply greyed).
- 181 unit tests (1 new: Follow grooves round a cylinder, apart and overlapping into a
  ring, checked sound, the right amount removed and fast).

## 0.20.0 — Slice 11: Patterns (2026-11-13)

- **Pattern** (Design → Features, and Build → Arrange): select a shape and choose what to
  repeat:
  - **The whole shape**: copies side by side, kept together as one object;
  - **Its last step**: the hole set, pad, pocket, revolve, groove, Follow groove /
    ridge, or any shape merged or subtracted, repeated on the same part (the choice
    shown by name; it starts here when the last step is one of those).
  Then **In a row** (direction, the other way, how many, spacing), **In a grid** (two
  directions) or **Round a circle** (turning round a line across / front to back /
  straight up through a centre you set; how many; 360° spreads them evenly, less
  spreads them over that angle). Live preview.
- **Linked**: change the original (the hole's size, the pad's sketch, the groove's
  path) and every copy follows. It's a step in the history: double-click to change
  the count, spacing or circle. Resizing the shape stretches the spacing with it.
  Roll back to an earlier feature to repeat that one instead.
- File format version 16. Older files still open.
- 180 unit tests (2 new: rows, grids and circles of shapes; holes repeated and linked,
  through all, round a circle; a Follow groove repeated).

## 0.19.1 — Follow no longer hangs off the edge (2026-11-12)

- A Follow path that runs off the edge of the face it's drawn on (a circle bigger than
  the box, or a shape made smaller than its groove) froze Forge3D. Now the path is
  checked as it's laid: where the surface turns a sharp corner, or the path can go no
  further, it stops at once with "The path runs off the edge of the surface…". In the
  Follow panel that shows as the message (Apply greyed); after resizing, the shape
  shows without the groove and the step is marked ⚠ with the reason, as other
  steps that can't be made. Rounded edges are still crossed.
- Long paths are laid faster (fewer points in each fitted curve).
- 178 unit tests (1 new).

## 0.19.0 — Slice 10: Follow (grooves and ridges along a path) (2026-11-12)

- **Follow** (Design tab → Features): pick a shape, click the spot on its surface
  where the path goes, and draw the path in the sketcher (lines, arcs, circles,
  slots; joined or not; open or closed). Then choose:
  - **Groove** (cut in) or **Ridge** (raised);
  - the shape across the path: **Round**, **Square**, **V**, or **My own** (drawn in
    the sketcher: across to the sides, up out of the surface, the dashed line is the
    surface);
  - width and depth (or height).
  On a flat face the path stays exactly as drawn; on a curved surface it is laid over
  the surface from the spot you clicked, bending round with it (round a cylinder,
  over a dome). The cross-section stays upright to the surface all along. The path
  shows in orange with a live preview; clicking the shape again moves the path there.
- It's a step in the shape's history: double-click it to change the settings, the
  path or the cross-section. It stays on the same face when the shape is resized.
- File format version 15. Older files still open.
- 177 unit tests (2 new: straight, ring, ridge, V, drawn cross-section, closed
  outline with corners, resizing, saving, and a path round a cylinder).

## 0.18.0 — Slice 9: middle planes and the ribbon (2026-11-12)

- **Sketch on a middle plane**: the Sketch button's arrow offers the front, side or top
  plane through the middle of the selected shape (or of the plate, with nothing
  selected). Where the plane cuts the shape is shown dashed, to see and snap to.
  This is what Groove and Revolve need for round parts: draw a groove's outline at
  a cylinder's edge, a helper line up its middle, and Groove cuts a ring all the way
  round. (On a flat face, Groove spins round a line lying on the face, which is why it
  made two dents there.)
- Pocket can now go **both ways** (half each side of the sketch), handy from a middle
  plane.
- **Ribbon**: the toolbar is now tabs of grouped buttons, so nothing falls off the
  edge:
  - **Build**: File, Undo, Add, Handles, Combine, Arrange, Remove;
  - **Design**: Sketch (with the planes), Hole, Shell, Edges, Split, Emboss;
  - **Sculpt**: coming in a later phase;
  - **Print**: Export, Send to Slicer.
  The tab you were on is remembered. Menus and shortcuts are unchanged.
- 175 unit tests (1 new: a groove round a cylinder from its front middle plane).

## 0.17.0 — Slice 8: Revolve, Hole, Shell (2026-11-12)

- **Revolve** (in the sketch's **Make:** list): spins the outline round a helper line
  (pick the line, or have just one): knobs, bottles, wheels. Angle, the other way,
  both ways. On the plate the sketch is the side view, standing up, and the shape sits
  on the plate. On a face, **Groove** cuts it in instead.
- **Hole** (new button): pick a shape, click a flat face where the holes go (each click
  adds one), then choose:
  - **Type**: plain, counterbored (flat step for a bolt head) or countersunk (cone for
    a flat-head screw);
  - **Size**: M2 to M12 (fills in every size), made for a bolt to pass through
    (clearance) or for cutting a thread (tap drill); or Custom, typed;
  - **Depth**, or **Through all**.
  Spots can be typed (mm from the face's middle), removed, or placed exactly in a
  sketch (**Place exactly...**: points with dimensions and rules). The preview shows
  the holes as they'll be. Double-click the Drilled step later to change them.
- **Shell** (new button): hollows the selected shape out to walls of a set thickness.
  Click the faces to leave open (the top of a box makes a cup), or none for a sealed
  hollow; walls inside or outside. Open faces are kept when the shape is resized.
  Double-click the step to change it.
- Sketcher: a **Point** tool (P), for hole spots; deleting a line keeps points that
  stand on their own.
- File format version 14. Older files still open.
- 174 unit tests (5 new).

## 0.16.0 — Slice 7: Sketches, Pad and Pocket (2026-11-12)

- **Sketch** (new button): draw a flat outline and push it out (**Pad**) or cut it in
  (**Pocket**). With nothing selected the sketch goes on the plate; with a shape
  selected, click the flat face to draw on (its outline is shown dashed to snap to).
- **Drawing**: Line (L, one after another; click the first point to close), Rectangle
  (R), Circle (C), Arc (A), Slot (S). Points snap to other points, the face's corners
  and the grid. Drag points, lines and circle edges to change them. **Helper** (G)
  turns lines into dashed helper lines that rules can use but the shape ignores.
- **Rules** on what's picked: Horizontal (H), Vertical (V), Parallel, Perpendicular,
  Equal (E), Tangent (T), Coincident (O), Symmetric, Lock. Lines drawn nearly level or
  upright get Horizontal / Vertical by themselves; rectangles and slots come with
  theirs. A rule that fights the others is refused.
- **Dimensions** (D): a line's length, two points' distance, a circle's radius, two
  lines' angle; **Across** (Shift+H) and **Up** (Shift+V) measure one way only (one
  point: from the sketch's origin). Typed when added; double-click one to change it,
  and the sketch follows.
- The sketch turns **green** when every size and place is set; the panel says how many
  ways it can still move. Loose ends of an open outline show in red.
- **Finish**: Pad (length, the other way, both ways) or, on a face, Pocket (depth,
  the other way, Through all). On a face the pad is merged into the shape (or the
  pocket subtracted), as one step in its history. Undo / Redo inside the sketch are
  the sketch's own; one Undo afterwards takes the whole pad or pocket away.
- **Change a sketch later**: double-click the pad (or its Pad / Pocket step) in Objects,
  or right-click → Change.... Resizing a pad with the handles scales its sketch.
- File format version 13 (sketches and pads). Older files still open.
- 169 unit tests (8 new).

## 0.15.1 — Slice 6 fixes (2026-11-12)

- **Rounded / chamfered edges survive resizing.** Each picked edge also remembers
  where it sits within the shape's box and which way its face points, so making the
  box under a rounding much taller (or wider) keeps the same edges rounded, and
  Change the edges... shows them picked. (A box rounded on top used to lose its
  rounding once it was made 8 mm or more taller.)
- **Subtract → Through all** (tick box in Properties): straight tools (box, cylinder,
  tube, text) reach right through the shape along their own height, whatever size the
  shape becomes. Works on imported meshes too.
- **Emboss → Center on the face**: All, Top-Bottom, Left-Right (as the words read).
  New words start in the middle of the face you're looking at. Clicking still moves
  them anywhere.
- 161 unit tests (3 new).

## 0.15.0 — Slice 6: History (2026-11-12)

- **The Objects panel shows how each object was made.** Open an object to see its
  steps, oldest first (Box 1 → Rounded 1 → Subtract 1), each with a short description
  ("4 edges rounded R2", "minus Cylinder 1"). Shapes a step used as tools are listed
  under it.
- **Double-click a step to change it**: a shape's size (in Properties), the edges of a
  rounding, the words of an emboss, a split's cut.
- **Right-click a step** to switch it off or on, change a Merge / Subtract / Intersect
  into another, roll back to it, or delete it (tools go back on the plate).
- **Rollback bar**: drag it up the history to see the object as it was at an earlier
  step. Edges and Emboss added while rolled back go in at the bar. Drag the bar to the
  bottom (or right-click → Roll forward) to bring every step back. Merge, Split,
  Mirror and Group wait until the object is rolled forward.
- A step that can't be done any more (e.g. a rounding whose edges an earlier chamfer
  changed) is marked ⚠ with the reason and left out, instead of the object vanishing.
- File format version 12. Older files still open.
- 158 unit tests (4 new).

## 0.14.1 — Fillets on outside edges (coves) (2026-11-12)

- **Fillet now works on outside edges too:** it cuts a curved hollow (a cove), as if a
  cylinder of the radius, centred on the edge, were subtracted. Inside corners are
  filled as before; one Fillet can do both.
- Straight edges, round rims (a cylinder's top) and other curves all work. Picked edges
  that meet at a corner join smoothly.
- 154 unit tests.

## 0.14.0 — Slice 5e: Edges (2026-11-12)

- **Edges tool** (toolbar, Edit menu) for solid shapes, with four kinds:
  - **Rounding**: outside edges turned into a curve of a set radius (R1, R2...).
  - **Fillet**: inside corners filled with a curve of a set radius.
  - **Chamfer**: the corner cut off flat, the same distance back on both faces.
  - **Bevel**: a flat slope a set width across the face you pick, at a set angle to it.
- Pick edges in the view: they light up as the mouse passes; click to pick or drop
  one. Click a face to pick all its edges (click again to drop them). **All edges**
  picks every edge the kind can go on. Rounding only offers outside edges and Fillet
  only inside corners.
- The view shows the result as you pick and change sizes; sizes too big for the
  shape get a plain message instead.
- The result stays editable: **Change the edges...** in Properties (kind, size, other
  edges), or **Sharp edges again** to get the plain shape back. Picks follow the shape
  when it is moved, resized, or the shape under it is changed.
- Not for imported meshes (or shapes made with them): a message says so.
- File format version 11. Older files still open.
- 154 unit tests (3 new).

## 0.13.1 — Slice 5d fixes: Emboss on balls and big models; stacked letters (2026-11-12)

- **Words keep their size on a ball.** Embossed words used to squeeze together and
  break up towards the ends of a long line on a sphere. The baseline now runs round the
  surface first and each letter stands straight up from it, like a sticker smoothed on.
- **Emboss on big models is quick.** Finding where the letters go only looks at the
  triangles near them and follows the surface from triangle to triangle. On shapes over
  150,000 triangles the preview shows the words in orange on top of the shape, and the
  cut is made once, when you click Emboss / Apply.
- Words longer than the way round a shape (e.g. right round a small box) are refused
  instead of running into themselves.
- **Stack the letters (top to bottom)**: a new tick box for the Text shape and Emboss.
  Each letter stands upright under the one before, like a sign on a post.
- Saved files keep the stacked setting (older files open as before).
- 151 unit tests.

## 0.13.0 — Slice 5d: Text and Emboss (2026-11-11)

- **Text shape** (Add → Text): 3D letters lying flat on the plate, in any font installed
  on the PC. Properties has the words, font, Bold, Italic and Letter height; Width,
  Depth and Thickness stretch it like a box. Typing more words makes the block longer;
  the letters keep their size.
- **Emboss** (toolbar, Edit menu): puts raised or engraved words on the selected shape.
  The words start on the side facing you; click anywhere on the shape to move them.
  Choose the words, font, bold / italic, letter height, depth, turn, and raised or
  engraved; the view shows the result as you change things.
- Embossed words **follow the surface**: each letter stands the same depth out of (or
  into) the surface under it, and lines wrap round curves such as a cylinder or ball,
  keeping their width. Works on imported meshes too.
- Words that would hang off the edge of a shape are refused with a message in the panel.
- The result stays editable: select it and click **Change the words...** in Properties
  (or **Remove the words** to get the shape back). Resizing it evenly resizes the words.
- File format version 10 (text shapes and emboss results). Older files still open.
- Core: `Text.h` (fonts, `makeText`, `withText`, `flatLetters`), `Emboss.h`
  (`emboss::letters`, `surfaceUnder`, `embossSpec`, `withEmboss`),
  `Document::previewMesh()`, `TextSpec`, `EmbossText`, `BooleanOp::Emboss`.
- 151 unit tests (4 new).

## 0.12.0 — Slice 5c: Mesh engine (2026-11-11)

- **Big imported meshes now combine quickly.** Merge, Subtract, Intersect, Split,
  Mirror and Group on an imported shape (or anything made with one) use a mesh engine
  (Manifold) instead of the solid-modelling kernel. Subtracting a box from a 1.3 million
  triangle scan takes about 4 seconds the first time (most of that is reading the mesh
  in once) and about 1 second after; it used to freeze.
- Moving a shape inside such a result re-cuts it in about a second.
- The "this may take minutes" question from 0.11.0 is gone: it's no longer needed.
- Import repair is about 2.5x faster on big meshes (1.3M triangles: under 2 s).
- Exporting big meshes is about 5x faster.
- Primitives combined with a mesh are turned into fine triangles (within 0.01 mm), so
  results are watertight and export cleanly.
- Core: `MeshKernel.h` (`MeshSolid`, `meshkernel::` operations),
  `Document::usesMeshEngine()`, `meshSolid()`, `localBounds()`, `previewResult()`.
  New dependency: Manifold 3 (Apache-2.0).
- 147 unit tests (4 new).

## 0.11.0 — Slice 5b: Import with repair (2026-11-11)

- **Import** (File menu, toolbar, Ctrl+I): adds shapes from **STL** (binary or text),
  **3MF** (every object, with its name; components, transforms and units handled) and
  **OBJ** (one shape per object / group; polygons split into triangles). Pick several
  files at once, or **drag files onto the window** (dropping a .f3d opens it).
- **Automatic repair** on the way in: duplicate points joined, empty and repeated
  triangles removed, triangles turned to match their neighbours, inside-out pieces
  turned the right way, holes filled. A message lists what was repaired.
- Imported shapes land on the plate, to the right of what's there (several shapes from
  one file keep their places relative to each other), named after the file or object.
- They work like any shape: move, turn, **resize** (Width / Depth / Height, handles,
  Uniform), Mirror, Duplicate, Group, Align, Export, and **Merge / Subtract / Intersect /
  Split** with other shapes. One that can't be made watertight can be moved, resized
  and exported, but not combined or split (Properties says so).
- Saved inside the .f3d file (file format version 9). Older files still open.
- **Units**: STL and OBJ files don't record their units. If one comes in under 2 mm
  across (usually saved in metres, e.g. from Blender, or in inches), Forge3D asks
  which units it was made in and scales it (metres x1000, inches x25.4, cm x10, or keep).
- Typing a size now always changes the field you typed in. (Before, a size the box
  couldn't show exactly, such as 0.003 mm, could be changed instead, and your number
  seemed to reset.)
- Combining, splitting, mirroring or grouping more than 20,000 triangles of imported
  mesh asks first, since it can take minutes, and logs when it starts.
- Sample files for testing are in `test-sheets/samples`.
- Core: `MeshSpec` / `MeshData`, Import.h (`readStl`, `readObj`, `read3mf`,
  `repairMesh`, `importFile`), `makeMeshSolid()`.
- 142 unit tests (4 new).

## 0.10.0 — Slice 5a: Export and Send to Slicer (2026-11-11)

- **Export** (File menu, toolbar, Ctrl+E): saves the selected shapes, or everything if
  nothing is selected, as **3MF** (best for slicers: keeps names and millimetres),
  **STL** (binary) or **OBJ**. Fine triangles (within 0.01 mm of the true surface);
  points shared between triangles so the files are watertight.
- **Send to Slicer** (File menu, toolbar): writes a 3MF to a temporary folder and opens
  it in your slicer. The first time, pick from the slicers found on the PC (PrusaSlicer,
  Bambu Studio, OrcaSlicer, Cura, Creality Print, SuperSlicer), the program Windows uses
  for .3mf files, or browse to any program. **File > Choose Slicer...** changes it.
- If a shape's surface isn't closed, Export says which one.
- The folder and file type used last are remembered.
- Core: Export.h (`exportMeshes`, `writeMeshes`, `formatForPath`).
- 138 unit tests (4 new); the 3MF files were also checked with an independent reader
  (trimesh): watertight, right way out, names kept.

## 0.9.0 — Slice 4c: Duplicate, Mirror, Group, move several (2026-11-11)

- **Duplicate** (Ctrl+D, toolbar): copies of the selected shapes, placed to the right of
  everything on the plate. Combined shapes are copied with all their parts.
- **Copy / Cut / Paste** (Ctrl+C / Ctrl+X / Ctrl+V, Edit menu). Paste after Copy lands
  clear of everything; the first Paste after Cut puts the shapes back where they were.
- **Mirror** (toolbar dropdown, Edit menu): flip the selected shapes in place, left /
  right, front / back or upside down. "Remove the mirror" (Properties) flips it back.
- **Group** (Ctrl+G): shapes move, turn and resize as one without merging. **Ungroup**
  (Properties, Separate, or Ctrl+Shift+G).
- **Several shapes at once**: with two or more selected, the Move and Rotate handles
  appear at their shared middle and move / turn them together (one Undo); dragging one
  of them on the plate slides them all. To resize several together, Group them.
- Fixed: long hints (e.g. "2 shapes selected...") no longer widen the Properties panel.
- File format version 8 (Mirror and Group). Older files still open.
- Core: `BooleanOp::Mirror` / `Group`, Arrange.h (`captureTree`, `cloneTree`,
  `addCopies`, `clearOfPlate`, `mirrorBodies`, `groupBodies`).
- 134 unit tests (5 new).

## 0.8.0 — Slice 4b: handles everywhere (2026-11-11)

- **Parts inside combined shapes get handles.** Select a part in Objects (e.g. the
  cylinder inside "Merge 1"): Move, Rotate and Resize (W / E / R) work on it in place,
  along the plate's directions, and the combined shape rebuilds as you drag. Esc during
  a drag puts it back; one Undo per drag.
- **Combined shapes get Resize handles.** Drag a face handle to stretch the whole shape;
  the opposite side stays still. Everything inside stretches with it (a round peg on a
  block stretched along X becomes oval). With **Uniform** on, it scales evenly.
- Stretching one way only isn't possible when a part inside is turned at an odd angle
  (or for a torus across its ring): the shape then scales evenly and the status bar
  says why.
- Split halves stretch with their cut.
- Core: `scaledTree()` / `scaledSpec()` (Scale.h), `EditBodiesCommand`,
  `movedOnPlate()` and `rotatedAboutPlateAxis()` for parts inside a shape.
- 129 unit tests (6 new).

## 0.7.1 — Drag the Split plane (2026-11-11)

- In the Split tool, **drag the yellow plane** (or its arrow) to slide it along its
  direction. Snaps to 1 mm; hold Alt to move freely; Esc during the drag puts it back.
  The slider and the "From the middle" field follow.
- While the Split tool is open, clicking in the view no longer changes the selection
  (and so no longer closes the tool by accident). Cancel or Esc leaves.
- After a split, the arrow-side half is selected on its own, so the Move / Rotate /
  Resize handles (and W / E / R) work on it straight away, as after Merge.

## 0.7.0 — Slice 4a: Split (2026-11-11)

- **Split** (toolbar and Edit menu): select one shape, click Split, and a see-through
  yellow plane with an arrow appears through its middle. In the panel on the right:
  - **Direction**: Flat, Across X, Across Y, or any tilt around X and Y.
  - **Position**: a slider (end to end of the shape) or a typed distance.
  - **Keep**: both halves as two shapes, or only one side.
  - **Split** cuts; **Cancel** or Esc leaves without cutting. One Undo puts the shape back.
- Halves are named after their side ("Box 1 (top)", "Box 1 (bottom)", left/right,
  front/back) and stay editable: open one in Objects to change the original shape.
- **Move the cut...** (Properties of a half) reopens the plane to change where that half
  was cut. **Remove the cut** gives back the whole shape.
- Splitting a combined shape works too; keeping both halves copies all its parts.
- File format version 7. Older files still open.
- Fixed: the "empty plate" hint still mentioned Add Box / Add Cylinder.
- Core: `BooleanOp::Split` with a plane, `splitBody()`, `duplicateTree()`,
  `Document::setBoolean()` (EditBodyCommand now updates a result's settings too).
- 123 unit tests (8 new).

## 0.6.1 — Oval shapes and the pyramid (2026-11-11)

- **Pyramid** in Add Shape: base width and depth, top width and depth, height. Top 0 x 0
  is a point; one of them 0 is a roof-like ridge; both above 0 is a flat top.
- **Oval radii** for every round shape, like the cylinder already had:
  - Sphere: Radius X, Y and Z (eggs and squashed balls).
  - Cone: bottom radius X / Y and top radius X / Y.
  - Torus: ring radius X / Y (an oval ring), tube radius across and up (Z).
  - Tube: outer radius X / Y and inner radius X / Y.
- The **Round** checkbox now appears for all of these. Ticked, the X / Y radii (a
  sphere: all three) change together, in Properties and with the Resize handles; ticking
  it on an oval shape makes it round again. Unticked, each radius changes on its own.
- File format version 6. Files from 0.6.0 (format 5) still open.
- 115 unit tests (4 new; the shape tests now cover every oval and pyramid variant).

## 0.6.0 — Slice 3d: shape library (2026-11-11)

- **Add Shape** dropdown in the toolbar (and a new **Add** menu) replaces Add Box / Add
  Cylinder: Box, Cylinder, **Sphere**, **Cone**, **Torus**, **Tube**, **Wedge**.
- Every new shape has its own Size fields in Properties, works with the Resize handles
  (a sphere stays a sphere; a cone keeps its taper; a tube keeps its wall thickness; a
  torus side handle widens the whole ring, the top handle thickens it), Uniform scale,
  Merge / Subtract / Intersect, Align Shapes, Save and Open.
- A cone's top (or bottom) radius can be 0 for a point, or more for a flat cut-off top.
- Sizes that can't work are refused with a message (e.g. a tube's hole bigger than the
  tube, a torus with no hole).
- File format version 5 (adds the new shape types). Older files still open.
- Core: one table per shape (`specFields()`, `specValue()`, `setSpecValue()`, `scaled()`,
  `defaultSpec()`), so the panel, files and scaling handle every shape the same way;
  `halfExtents()`.
- 111 unit tests (7 new).

## 0.5.1 — Turning centre and Align Shapes (2026-11-11)

- **Combined shapes turn about what you see.** Gizmo rings and typed angles turn a
  Merge, Subtract or Intersect result about the centre of its actual solid; cut-away
  shapes don't count. A cube with a taller cylinder subtracted turns about the cube's
  middle. The gizmo sits there too.
- **Align Shapes** (toolbar dropdown and Edit menu). Select shape 1, Ctrl+click shape 2
  (or more), then pick: **Meet At Face**; **Align Face Center XYZ / X / Y / Z** (halfway
  in); **Center XYZ / X / Y / Z**. Shape 1 stays put. One Undo step per align.
- Shape bounds are now exact for tilted shapes, so **Sit on plate** puts a tilted cylinder
  exactly on the plate.
- Core: `Document::visibleCentre()`, `shapeBounds()`, `withRotation()`, a
  `rotatedAboutPlateAxis()` overload taking a centre, and `Align.h`.
- 104 unit tests (8 new).

## 0.5.0 — Slice 3c: combine shapes (2026-11-11)

- **Multi-select**: Ctrl+click in the view or the Objects list adds or removes shapes.
  The order you pick them in is kept.
- **Merge**, **Subtract** (keeps the first shape picked, cuts the others out of it) and
  **Intersect** in the toolbar and Edit menu. Exact solids from OpenCASCADE, with faces on
  the same surface unified so merges have no seams. Combining shapes that would leave
  nothing (e.g. Intersect of shapes that don't touch) is refused with a message.
- **Results stay editable**: the Objects panel is now a tree; a result holds its parts.
  Select a part to see it see-through in place and edit its size and position in
  Properties; the result rebuilds. Results can be combined again (nesting).
- **Separate** (toolbar or Properties) splits a result back into its parts, each left
  where it appeared, even after the result was moved or turned.
- Move and Rotate work on results with the gizmo; Resize handles are not shown for results.
- Delete removes a result with all its parts; parts can't be deleted on their own.
- File format version 4 stores results (operation, parts, pivot).
- Core: `BooleanSpec` on `Body`; the document tracks which result each part belongs to,
  caches exact shapes, and rebuilds results when a part changes. `CombineCommand`,
  `SeparateCommand`, `placementFromTransform()`.
- 96 unit tests (13 new).

## 0.4.2 — Fix (2026-11-11)

- Round checkbox now unticks when Undo, Redo or a typed radius turns the selected cylinder
  oval (found in test sheet 3b2, check 15).

## 0.4.1 — Uniform scale and elliptical cylinders (2026-11-11)

- **Uniform scale**: toolbar / Edit menu toggle (key **U**) and a matching checkbox in
  Properties → Size. Resize handles and typed sizes then scale every dimension by the same
  factor. The opposite side stays put and the base stays on the plate.
- **Elliptical cylinders**: cylinders have **Radius X** and **Radius Y**. With **Round**
  ticked (the default for round cylinders), both change together; untick it to change
  each on its own. Ticking Round again makes the cylinder round using Radius X. Built as an
  exact elliptic extrusion in OpenCASCADE (a true circular cylinder when the radii match).
- Resize handles on a box's sides now keep the opposite side fixed at base level, so
  uniform scaling from a side handle does not push the body into the plate.
- File format version 3: cylinders store radiusX and radiusY. Older files open with round
  cylinders.
- Properties: no blank gap under a box's sizes after showing a cylinder.
- 83 unit tests (6 new).

## 0.4.0 — Slice 3b (2026-11-11)

- Left-drag any body to slide it across the plate (selects it too).
- Gizmo around the selected body, with three tools in the toolbar and Edit menu:
  **Move (W)** arrows along X / Y / Z, **Rotate (E)** rings around X / Y / Z (about the
  plate's axes, through the body's centre), **Resize (R)** a handle on each face; the
  opposite face stays put (cylinders: radius grows around the centre line).
- Hover highlight on handles; the dragged handle turns yellow and the others fade.
- Snapping (toolbar **Snap**, on by default): 1 mm moves, 15° turns, whole-mm sizes.
  Hold **Alt** while dragging to move freely.
- Every drag is one undo step (`EditBodyCommand`); **Esc** during a drag cancels it.
- Live status while dragging ("Move X 12.0 mm", "Rotate around Z 45°", "Resize Height 37 mm").
- Core: `eulerXYZ()`, `rotatedAboutPlateAxis()`, `resizedFace()`, `dimension()`.
- View: `Gizmo` (screen-constant geometry, hit testing) and drag maths
  (`closestOnLine`, `intersectPlane`, `screenAngle`, `snapTo`).
- 77 unit tests (18 new).

## 0.3.1 — Slice 3a revision (2026-11-11)

- Rotation about all three axes: Properties has a Rotation group (Around X, Y, Z). Turns
  are applied X, then Y, then Z, about the plate's fixed axes, around the body's centre.
- **Sit on plate** button: moves the selected body so its lowest point touches the plate.
- One transform (`core/Transform.h`, `placementTransform`) now drives drawing, picking and
  bounds, so what you see is exactly what you click.
- File format version 2: placements store rotationX, rotationY and rotationZ. Version 1
  files still open (no tilt).
- A file passed on the command line opens after the main window appears, so error
  messages have a window behind them.
- 59 unit tests (10 new: transforms, tilted bounds and picking, sit on plate, format v1/v2).

## 0.3.0 — Slice 3a (2026-11-11)

- Selection: left-click a body to select it (cyan glow); click empty space or press Esc to
  clear. New bodies are selected as they are added. Selection is not part of undo.
- Objects panel (left): every body by name; click to select.
- Properties panel (right): name, size (box width/depth/height, cylinder radius/height) and
  position (X, Y, lift, rotation). Each typed change is its own undo step.
- Delete key / Delete button removes the selected body.
- `.f3d` files: a zip holding `document.json` (format version 1, units mm). File → New, Open,
  Save, Save As; Save / Discard / Cancel before losing changes; file name in the title;
  last folder remembered. Saving writes a temporary file first, so a failed save never
  damages the previous file. Damaged, foreign or newer-format files give a clear message.
- Opening a `.f3d` passed on the command line (drag a file onto Forge3D.exe).
- Ray picking in core (`Picking.h`); `Document::replaceAll()`; `SetPlacementCommand` can be
  non-merging for typed values.
- Log records Undo, Redo, Saved and Opened.
- New dependencies: miniz (zip) and nlohmann-json.
- 49 unit tests (15 new: picking, file format, camera rays, typed moves).

## 0.2.0 — Slice 2 (2026-11-10)

- `Document`: an ordered list of bodies (box or cylinder parameters, placement, name),
  with change notifications, unique naming, input validation, and a geometry cache that
  re-tessellates only when a body's parameters change.
- Commands: Add, Delete, Move, Resize, Rename, and Macro (several changes as one step).
- `CommandStack`: one undo history for everything, with merging of repeated moves
  (for drags), macros, a 200-step limit, and saved-state tracking.
- Viewport draws any number of bodies, each in its own colour, and re-uploads only
  meshes that changed.
- Edit menu and toolbar: Undo / Redo with labels ("Undo Add Box"), Ctrl+Z, Ctrl+Y and
  Ctrl+Shift+Z. `*` in the title for unsaved changes; File → New and closing ask first.
- Temporary test tools: Add Box, Add Cylinder, Make Last Taller, Delete Last, Clear Plate.
- Startup timing (`Startup:` lines) and one `Command:` line per edit in the log.
- `--demo-scene` start-up option for screenshots.
- 34 unit tests (21 new for the document and undo stack).

## 0.1.0 — Slice 1 (2026-11-10)

- CMake + vcpkg project with presets for Windows (Debug, Release) and Linux (dev checks).
- `forge_core`: box and cylinder primitives (OpenCASCADE) and B-rep → triangle mesh tessellation.
- `forge_view`: Z-up orbit camera (orbit, pan, zoom, fit).
- Forge3D app: Qt 6 window with an OpenGL 3.3 viewport, 220 mm build-plate grid with X/Y axes,
  and a 40 × 30 × 20 mm starter box.
- Navigation: right-drag orbit, middle-drag or Shift+right-drag pan, wheel zoom, F fit, Home reset.
- Log file and Windows crash dumps under `%LOCALAPPDATA%\Forge3D`.
- `--screenshot <file>` option for automated visual checks.
- 13 unit tests (primitives, tessellation, camera).
- GitHub Actions Windows build that runs the tests and uploads a ready-to-run zip.
