// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Emboss.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
#include <string>
#include <vector>

namespace forge {

class Document;

// Quick Hinge: hinges and swivels that fit and turn once printed.
//
// Everything is laid out from the spot on the fork's surface: the pin lies across the
// surface (along `pinAxis`), its middle `pinDepth` in from the surface, and the tab (or
// the swivel's peg) comes out of the fork along the surface's normal.
namespace hinge {

// Throws std::invalid_argument (in plain words) if the settings can't make the joint.
void check(const HingeFinish& f);

// How far the slot and the pin hole reach out each side of the spot (`reach`, or auto).
double reachOf(const HingeFinish& f);
// How far in from the surface the pin's middle is.
double pinDepth(const HingeFinish& f);
// The way the pin lies, for a spot facing `normal` (turned by f.turn).
Vec3 pinAxis(const HingeFinish& f, const Vec3& normal);
// The pin holes' diameters: through the fork's arms (the pin stays put in them), and
// through the tab or link (where it turns: snug if stiff, else with room).
double armHole(const HingeFinish& f);
double tabHole(const HingeFinish& f);

// The fork half (Swivel: the bore): what's cut away, as closed triangle surfaces.
std::vector<Mesh> forkCutters(const HingeFinish& f, const Vec3& point, const Vec3& normal);

// The tab half made from another part: the room cleared round it (everything of that
// part inside the fork's reach, and `fit` clear of the fork's surface), the tab (Swivel:
// the peg) added, then its pin hole (Swivel: the slit) cut.
struct TabHalf {
    Mesh clear;
    Mesh add;
    std::vector<Mesh> cut;
};
TabHalf tabHalf(const HingeFinish& f, const Vec3& point, const Vec3& normal);

// Shapes to print with the joint, in place (plate coordinates), each a closed surface:
// the tab (Single, when no other part is made into it), the link (Double), the peg
// (Swivel, likewise), and a printed pin (Printed) reaching `pinSpan` either way of the
// spot along the pin (to the part's surface, say; {0, 0}: the reach both ways).
struct Piece {
    std::string name;  // "Hinge tab 3 × 5", "Hinge link", "Hinge pin Ø2 × 9.4", "Swivel peg Ø4"
    Mesh mesh;
};
std::vector<Piece> pieces(const HingeFinish& f, const Vec3& point, const Vec3& normal, bool withTab,
                          std::array<double, 2> pinSpan = {0.0, 0.0});

// The other half of a hinge made on another part: the tab (Swivel: the peg) at the
// same spot; for a double hinge, the second fork, facing back, where the link's other
// pin is (the pin lying the same way).
struct Half {
    Vec3 point, normal;
    HingeFinish finish;
};
Half otherHalf(const HingeFinish& f, const Vec3& point, const Vec3& normal);

// For the history: "Hinge fork 3 × Ø5, pin Ø2", "Hinge tab...", "Swivel bore Ø4 × 5"...
std::string describe(const HingeFinish& f);

// A closed surface (plate coordinates) as an imported-style shape: the mesh data,
// centred like a primitive, and where it goes.
Body pieceBody(const Mesh& mesh, const std::string& name);

}  // namespace hinge

// The Hinge step for `target` (a shape on the plate, or a part inside one) at a spot in
// plate coordinates. Preview it with Document::previewMesh, add it with CombineCommand.
BooleanSpec hingeSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                      const HingeFinish& finish);
// Where a Hinge step's spot is, in plate coordinates.
emboss::SurfacePoint hingeSpot(const Document& doc, ObjectId result);
// The Hinge step with new settings and / or a new spot (plate coordinates).
Body withHinge(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const HingeFinish& finish);
// The other half of the same hinge (the step with the same link number), if any.
ObjectId hingePartner(const Document& doc, ObjectId result);
// The shapes made to go with the Hinge step `step` (its tab, link, peg, pins) remade for
// `f` at `spot`: each as it should now be, those that change only. A piece left where it
// was made moves with the joint; one that's been moved stays where it is.
std::vector<Body> refreshedPieces(const Document& doc, ObjectId step, const HingeFinish& f,
                                  const emboss::SurfacePoint& spot);

}  // namespace forge
