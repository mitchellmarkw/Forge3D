// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
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
    Draw,     // push the surface out along the area's facing (in, inverted)
    Smooth,   // even it out
    Inflate,  // swell each point out along its own facing, like a balloon (shrink, inverted)
    Flatten,  // press towards the area's average plane
    Pinch,    // draw points together towards the middle, for sharp creases (apart, inverted)
    Clay,     // build up in flat layers (dig, inverted)
    Grab,     // pull the area along with the mouse (see beginGrab)
};

// Mirror planes through `centre`, each across one axis: what the brush does on one side
// happens on the other too.
struct Symmetry {
    bool x = false;
    bool y = false;
    bool z = false;
    Vec3 centre{0.0, 0.0, 0.0};
    bool any() const { return x || y || z; }
    // The point and its mirror images (itself first; 1, 2, 4 or 8 in all).
    std::vector<Vec3> images(const Vec3& p) const;
    // Mirrors a direction (or offset) the same way as images() mirrors the i-th point.
    Vec3 mirrored(const Vec3& v, std::size_t image) const;
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
    // One dab of a brush (not Grab). Returns the range of points changed, [first, last)
    // (empty if none); their normals are updated too.
    std::pair<std::uint32_t, std::uint32_t> apply(const Dab& dab, std::uint32_t nearTriangle);
    // The same dab, and its mirror images if `symmetry` has any. The images start from
    // the point nearest each mirrored centre.
    std::pair<std::uint32_t, std::uint32_t> apply(const Dab& dab, std::uint32_t nearTriangle,
                                                  const Symmetry& symmetry);
    // The point nearest `p` (a brute-force search).
    std::uint32_t nearestPoint(const Vec3& p) const;

    // Grab: on press, the points under the brush (and their mirror images) are taken
    // with how much each follows; each move puts them at where they were + offset x
    // weight (the offset mirrored for the images).
    void beginGrab(const Vec3& centre, double radius, std::uint32_t nearTriangle, const Symmetry& symmetry);
    std::pair<std::uint32_t, std::uint32_t> moveGrab(const Vec3& offset);
    void endGrab();
    // Puts every point back as given (an undo); normals recomputed.
    void setPositions(const std::vector<float>& positions);

private:
    std::pair<std::uint32_t, std::uint32_t> normalsAround(const std::vector<std::uint32_t>& moved);
    void normalsOf(const std::vector<std::uint32_t>& points);
    std::vector<std::uint32_t> pointsWithin(const Vec3& centre, double radius, std::uint32_t nearTriangle);
    std::vector<std::uint32_t> pointsFrom(const Vec3& centre, double radius, const std::vector<std::uint32_t>& seeds);
    std::pair<std::uint32_t, std::uint32_t> applyFrom(const Dab& dab, const std::vector<std::uint32_t>& seeds);

    struct Grabbed {
        std::uint32_t point;
        float weight;
        std::array<float, 3> from;
        std::uint8_t image;
    };
    std::vector<Grabbed> m_grab;
    Symmetry m_grabSymmetry;

    Mesh m_mesh;
    // Neighbours of each point, and the triangles round it (compressed rows).
    std::vector<std::uint32_t> m_neighbourStart, m_neighbours;
    std::vector<std::uint32_t> m_triangleStart, m_triangles;
    std::vector<std::uint32_t> m_mark;  // for walks: the walk number that last reached it
    std::uint32_t m_walk = 0;
};

}  // namespace forge::sculpt
