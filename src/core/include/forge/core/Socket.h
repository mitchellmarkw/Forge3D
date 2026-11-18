// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Emboss.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <string>
#include <vector>

namespace forge {

class Document;

// Quick Socket: ball and peg sockets that fit once printed.
namespace socket {

// How a ball sits in its socket: the hollow's radius (the ball's, plus the fit), how far
// below the opening the ball's middle is, and the opening's diameter.
struct BallSeat {
    double radius = 0.0;
    double centreDepth = 0.0;
    double opening = 0.0;
};
BallSeat ballSeat(const SocketFinish& f);

// How far down from the opening the relief cuts go (reliefDepth, or its "auto" value).
double reliefDepth(const SocketFinish& f);

// Throws std::invalid_argument (in plain words) if the settings can't make a socket.
void check(const SocketFinish& f);

// What's cut away, as closed triangle surfaces (one for the hollow, one per relief cut),
// for a socket whose opening is at `point` on a surface facing out along `normal`. The
// hollow starts a little outside the surface, so it opens cleanly on a curved one.
std::vector<Mesh> cutters(const SocketFinish& f, const Vec3& point, const Vec3& normal);

// For the history: "Ball socket Ø3 (snap-in)", "Hexagon peg socket 3 × 4"...
std::string describe(const SocketFinish& f);

// The ball or peg the socket takes, at its true size (the fit is all in the socket), to
// add as a shape of its own, and a name for it ("Ball Ø3", "Hexagon peg 3 × 4").
PrimitiveSpec matchingPart(const SocketFinish& f);
std::string matchingName(const SocketFinish& f);

}  // namespace socket

// The Socket step for `target` (a shape on the plate, or a part inside one) with the
// opening at a spot given in plate coordinates. Just its recipe: preview it with
// Document::previewMesh, then add it with CombineCommand (or InsertStepCommand).
BooleanSpec socketSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                       const SocketFinish& finish);
// Where a Socket step's opening is, in plate coordinates.
emboss::SurfacePoint socketSpot(const Document& doc, ObjectId result);
// The Socket step with new settings and / or a new spot (plate coordinates).
Body withSocket(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const SocketFinish& finish);

}  // namespace forge
