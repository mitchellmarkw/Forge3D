// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <optional>
#include <vector>

// Sculpting (prototype): brushes that push and smooth a dense triangle mesh.
namespace forge::sculpt {

// Points at the same place (within `tolerance`) made one, so the triangles share them
// (a tessellated solid has a copy of each corner per face). Normals are recomputed.
Mesh welded(const Mesh& mesh, double tolerance);

// Every edge longer than maxEdge split in half, again and again, until none is (or the
// mesh reaches maxTriangles, when it stops early). Shared edges are split once, so the
// surface stays closed.
Mesh refined(const Mesh& mesh, double maxEdge, std::size_t maxTriangles);

// The edge length that gives a mesh of about `triangles` triangles over this area.
double edgeForTriangles(const Mesh& mesh, std::size_t triangles);

enum class Brush {
    Draw,    // push the surface out (in, inverted)
    Smooth,  // even it out
};

struct Dab {
    Vec3 centre;         // on the surface
    double radius = 5.0;
    double strength = 0.5;  // 0..1
    Brush brush = Brush::Draw;
    bool invert = false;    // Draw: push in
};

struct Hit {
    Vec3 point;
    Vec3 normal;  // of the triangle hit
    std::uint32_t triangle = 0;
};

// A mesh being sculpted: its points move in place; the triangles never change.
class Sculptor {
public:
    explicit Sculptor(Mesh mesh);

    const Mesh& mesh() const { return m_mesh; }
    // Where a ray first meets the surface.
    std::optional<Hit> hit(const Vec3& origin, const Vec3& direction) const;
    // One dab of a brush. Returns the range of points changed, [first, last) (empty if
    // none); their normals are updated too.
    std::pair<std::uint32_t, std::uint32_t> apply(const Dab& dab, std::uint32_t nearTriangle);
    // Puts every point back as given (an undo); normals recomputed.
    void setPositions(const std::vector<float>& positions);

private:
    void normalsAround(const std::vector<std::uint32_t>& points);
    std::vector<std::uint32_t> pointsWithin(const Vec3& centre, double radius, std::uint32_t nearTriangle);

    Mesh m_mesh;
    // Neighbours of each point, and the triangles round it (compressed rows).
    std::vector<std::uint32_t> m_neighbourStart, m_neighbours;
    std::vector<std::uint32_t> m_triangleStart, m_triangles;
    std::vector<std::uint32_t> m_mark;  // for walks: the walk number that last reached it
    std::uint32_t m_walk = 0;
};

}  // namespace forge::sculpt
