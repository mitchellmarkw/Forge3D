// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Paint: colours brushed onto one shape (skin, clothes, eyes), shown in the view and
// exported to 3MF for colour printers. The shape itself stays as it is (a shape with few
// triangles is divided finer, so the paint's edges show).
namespace forge {

class Document;

namespace paint {

// No paint: the shape's own colour.
inline constexpr std::uint32_t kOwnColour = 0xFFFFFFFFu;

// Which colour is where, quickly: the dabs filed in a grid as big as the largest.
class Field {
public:
    explicit Field(const std::vector<PaintDab>& dabs);
    // The last dab's colour over p (kOwnColour if none, or rubbed out).
    std::uint32_t colourAt(const Vec3& p) const;
    bool empty() const { return m_dabs.empty(); }

private:
    std::vector<PaintDab> m_dabs;
    double m_cell = 1.0;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_cells;
};

// Throws std::invalid_argument (in plain words) if it can't be used.
void check(const PaintFinish& f);
// "3 colours" / "rubbed out"
std::string describe(const PaintFinish& f);
// The colours brushed on, in the order first used (rubbing out isn't one).
std::vector<std::uint32_t> colours(const std::vector<PaintDab>& dabs);

// Each triangle's colour from its middle (kOwnColour: the shape's own). `toPaint` takes
// the mesh's points to the dabs' coordinates.
std::vector<std::uint32_t> triangleColours(const std::vector<float>& positions,
                                           const std::vector<std::uint32_t>& indices, const Field& field,
                                           const Transform& toPaint = Transform::identity());
// Mesh::paint from each triangle's colour (each triangle's corners take it; a mesh whose
// triangles share points blends at the edges).
void colourMesh(Mesh& mesh, const std::vector<std::uint32_t>& triangleColours);

// The surface the paint goes on: a shape with few triangles divided finer (to about 60
// thousand), anything else as it is. Positions and indices.
Mesh paintSurface(const Mesh& mesh);
// The Paint step's shape.
MeshSolid apply(const MeshSolid& solid, const PaintFinish& f);

// A mask (Split by Mask) of where one colour shows: its dabs paint, every other dab rubs
// out.
std::vector<MaskDab> maskFor(const std::vector<PaintDab>& dabs, std::uint32_t colour);
// Dabs moved by a rigid transform.
std::vector<PaintDab> moved(const std::vector<PaintDab>& dabs, const Transform& t);

// Every Paint step (switched on) in an object's history, oldest first, its dabs in the
// object's own coordinates: what colours its shape.
std::vector<PaintDab> objectPaint(const Document& doc, ObjectId top);

}  // namespace paint

// A Paint step on `target` (dabs in plate coordinates).
BooleanSpec paintSpec(const Document& doc, ObjectId target, const std::vector<PaintDab>& inPlate);
// A Paint step's dabs in plate coordinates (to change).
std::vector<PaintDab> paintInPlate(const Document& doc, ObjectId result);
// The step with changed dabs (plate coordinates).
Body withPaint(const Document& doc, ObjectId result, const std::vector<PaintDab>& inPlate);

}  // namespace forge
