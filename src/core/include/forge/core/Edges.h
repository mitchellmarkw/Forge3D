// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace forge {

class Document;

// Rounding, fillets, chamfers and bevels on the edges of solid shapes (OpenCASCADE
// BRepFilletAPI). Edges are picked by clicking them (or a face, for all its edges) and
// stored by place (EdgeRef), so they are found again when the shape is rebuilt.
namespace edges {

enum class Bend {
    Convex,   // an outside edge (a box's edges)
    Concave,  // an inside corner (where a peg meets a plate)
    Smooth,   // faces meet without a corner: nothing to round
};

struct ShapeEdge {
    std::vector<Vec3> polyline;  // along the edge, for drawing and picking
    Vec3 mid{0, 0, 0};           // a point half way along it
    Bend bend = Bend::Smooth;
    std::array<int, 2> faces{-1, -1};  // the two faces it joins (indices into ShapeEdges::faceEdges)
    std::array<Vec3, 2> normals{};     // which way each of those faces points, at `mid`
};

struct ShapeEdges {
    std::vector<ShapeEdge> edges;
    std::vector<std::vector<int>> faceEdges;  // per face: its edges (indices into edges)
};

// The edges of a solid that a finish could go on (no seams, no edges with one face).
// Coordinates as the shape's own.
ShapeEdges analyse(const TopoDS_Shape& shape);

// Can this kind of finish go on an edge that bends like this?
bool suits(EdgeFinishKind kind, Bend bend);

// The face a ray from `origin` along `direction` meets first, and where.
struct FaceHit {
    int face = -1;
    Vec3 point{0, 0, 0};
    double distance = 0.0;  // along the ray
};
std::optional<FaceHit> faceUnder(const TopoDS_Shape& shape, const Vec3& origin, const Vec3& direction);
// The same, set up once for many rays (hovering).
class FaceFinder {
public:
    explicit FaceFinder(const TopoDS_Shape& shape);
    ~FaceFinder();
    FaceFinder(const FaceFinder&) = delete;
    FaceFinder& operator=(const FaceFinder&) = delete;
    std::optional<FaceHit> under(const Vec3& origin, const Vec3& direction) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// How to find edge `edge` again; `face`: the face it was picked from (-1: either).
EdgeRef refFor(const TopoDS_Shape& shape, const ShapeEdges& found, int edge, int face);
// Which edge a reference means (-1 if none is there any more).
int edgeFor(const ShapeEdges& found, const EdgeRef& ref, double tolerance);
// Notes where each reference sits within `part` (its box) and which way its face points
// (EdgeRef::located), so it can be found again when the part is resized. `part` and the
// references in the same coordinates.
void locate(const TopoDS_Shape& part, std::vector<EdgeRef>& refs);
// The edge each reference means now, in `found` (the edges of `shape`), or -1: the edge
// at the same place; else the one at the same spot in the shape's box (it was resized);
// else the nearest one this kind suits, if close. No edge is used twice.
std::vector<int> match(const TopoDS_Shape& shape, const ShapeEdges& found, const std::vector<EdgeRef>& refs,
                       EdgeFinishKind kind);

// The shape with the finish applied. Throws GeometryError with a plain message when
// it can't be done (a radius too big for the faces next to an edge, say).
TopoDS_Shape apply(const TopoDS_Shape& shape, const EdgeFinish& finish);

}  // namespace edges

// The Edges result's recipe for a shape on the plate (picks in plate coordinates):
// changes nothing in the document. Throws std::invalid_argument for an imported mesh.
BooleanSpec edgesSpec(const Document& doc, ObjectId target, const EdgeFinish& finish);
// An Edges result's finish with its picks in plate coordinates, and the result with a
// new finish given in plate coordinates.
EdgeFinish edgesInPlate(const Document& doc, ObjectId result);
// Which of the edges of pickableShape(doc, result) an Edges result's picks mean now
// (-1: not found), matched as the result itself does.
std::vector<int> pickedEdges(const Document& doc, ObjectId result);
Body withEdges(const Document& doc, ObjectId result, const EdgeFinish& inPlate);
// The shape in plate coordinates, as the Edges tool picks on it: the target's own
// shape, or the shape under the finish when changing an Edges result.
TopoDS_Shape pickableShape(const Document& doc, ObjectId target);

}  // namespace forge
