// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <vector>

namespace forge {

class Document;

// Screw threads (slice 26): cut into a round face of a solid, a peg or a hole. The
// thread is made as triangles (a helix swept round the face) and cut with the mesh
// engine, so the result is a mesh-engine shape.
namespace thread {

// Standard metric (ISO, coarse) sizes.
struct Size {
    std::string name;  // "M6"
    double diameter;   // 6
    double pitch;      // 1
};
const std::vector<Size>& metric();

// A round (cylindrical) face: where its axis is, how big, how long, and which way it
// faces (a hole faces in).
struct Round {
    bool hole = false;
    double radius = 0.0;
    Vec3 axisPoint{0, 0, 0};  // on the axis, where `from` and `to` count from
    Vec3 axis{0, 0, 1};       // unit
    double from = 0.0, to = 0.0;  // the face's extent along the axis
};
// Face `face` (FaceFinder numbering) of `shape`, if it's round.
std::optional<Round> roundFace(const TopoDS_Shape& shape, int face);
// The standard size that fits a round face best (a peg: its diameter; a hole: drilled
// for the thread, the thread's inside diameter).
Size suggest(const Round& round);

// What is cut away, as closed triangle meshes in the face's coordinates: `clear`
// (bringing a peg down to the thread's outside, or opening a hole up to its inside,
// where needed; may be empty) and the helical `groove`, already trimmed to the face's
// length.
struct Cut {
    Mesh clear;
    Mesh groove;
};
Cut cutFor(const Round& round, const ThreadFinish& finish);

}  // namespace thread

// A Thread step on `target`, the face picked in plate coordinates (like shellSpec).
BooleanSpec threadSpec(const Document& doc, ObjectId target, const ThreadFinish& inPlate);
// A Thread result's finish with its face in plate coordinates (to edit), and with a new one.
ThreadFinish threadInPlate(const Document& doc, ObjectId result);
Body withThread(const Document& doc, ObjectId result, const ThreadFinish& inPlate);
// The round face a Thread step is on, in its part placed in step coordinates.
std::optional<thread::Round> threadedFace(const Document& doc, const BooleanSpec& spec);

}  // namespace forge
