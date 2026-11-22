// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Emboss.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"

#include <string>

// Hand and accessory ports (slice 70): a standard wrist peg for swappable hands (the hand
// takes a Quick Socket sized to it), and grip holes through a hand sized to the handles
// of weapons and props (and pegs on the props sized to them).
namespace forge {

class Document;

namespace port {

// Throws std::invalid_argument (in plain words) if the settings can't be made.
void check(const PortFinish& f);
// For the history: "Wrist peg Ø3 × 4 (chamfered)", "Grip hole Ø3.2"...
std::string describe(const PortFinish& f);

// What's added (a peg) or cut away (a grip hole), as one closed surface. A peg starts
// inside the part, so it joins it on a curved surface too. `reach`: how far a grip hole
// goes on straight in (through the whole part).
Mesh shape(const PortFinish& f, double reach);
// The part with the peg added, or the grip hole cut through.
MeshSolid apply(const MeshSolid& part, const PortFinish& f);

// The socket a peg plugs into (for Quick Socket on the hand): a peg socket of its shape,
// size and length, or a snap-in ball socket for a ball end. `fit`, `resin`: the printer's.
SocketFinish socketFor(const PortFinish& peg, double fit, bool resin);
// A peg's sizes from a standard's socket joint (a peg socket, or a ball socket for a
// ball end); the rest as they are.
PortFinish fromSocket(const SocketFinish& joint, PortFinish now);
// The joint to keep in a standard for a peg (a peg socket, or a ball socket for a ball
// end) or a grip (a round peg socket of the handle's size).
SocketFinish jointFor(const PortFinish& f, bool resin);

}  // namespace port

// The Port step for top-level shape `target`, at a spot on it in plate coordinates (its
// step coordinates are the plate's). Preview it with Document::previewMesh, then add it
// with CombineCommand. Throws if `target` isn't a shape on the plate.
BooleanSpec portSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where, PortFinish f);
// A Port step's settings (its own coordinates).
const PortFinish& portOf(const Document& doc, ObjectId result);
// The Port step with new settings (its own coordinates). The caller records it.
Body withPort(const Document& doc, ObjectId result, const PortFinish& f);

}  // namespace forge
