# To do (not yet scheduled)

Ideas waiting for a place on the [roadmap](../ROADMAP.md), in the order to build them
(agreed 2026-11-20; joint standards, the first, is slice 67).
Each has what's wanted, then notes on how it could fit Forge3D.

## 1. Cut a sculpt into posable parts

Use the Pose Figure skeleton: pick the joints to cut and a joint type for each (ball,
hinge, swivel), and the one-piece figure is split there with matching sockets.

Notes: the skeleton's points give each cut's place and direction; Split (plane through the
joint, square to the bone) or Split by Mask; Quick Socket / Quick Hinge on the cut faces,
sized from the joint standard; the pieces named (Left forearm...).

## 2. Mould maker

A two-part mould box around a part, with registration keys, a pour spout and air vents,
for casting resin copies of a printed master.

Notes: a box round the part's bounds plus a wall; split at a parting plane (or the widest
outline); the part subtracted; keys like Split by Mask's; spout and vents as cylinders to
the top.

## 3. Hand and accessory ports

A standard wrist peg for swappable hands, and grip holes sized to that hand for weapons
and props.

## 4. Hand generator

Fists, open, gripping and pointing hands at a chosen size, made to fit the wrist peg.

## 5. Joint cut-outs for a target angle

Pose Test shows where parts bump; this trims just enough away for the joint to reach a
chosen angle (a 90° knee).

## 6. Texture stamps for sculpting

Press a picture into the surface as a brush: fabric weave, skin pores, scales, chainmail.

## 7. Linked left/right parts

Mirror an arm once; changes to the left carry over to the right.

## 8. "Will it stand?" check

The figure's centre of weight against its feet for the current pose, with a warning when
it would tip; a stand peg-hole helper in the feet.

## 9. Display bases

A base with peg holes that line up with the feet in the current pose.

## 10. A design variables table

Named values (`PegDia = 3`) used across many shapes, so changing one updates everything
(like FreeCAD's spreadsheet).

## 11. Instruction sheets

An exploded view with parts numbered to match the Parts List, saved as a PDF to go with a
kit.

## 12. Sewing patterns for soft goods

Flatten a painted area of the figure (a jacket, a cape) into a 2D pattern to cut cloth
from.

## 13. Pictures for sharing

A turntable GIF or a clean render of the model, for posts or a shop listing.
