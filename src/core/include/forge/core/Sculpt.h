// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

// Sculpting: brushes that push, smooth and mask a dense triangle mesh.
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
    Crease,   // cut a sharp groove: in and drawn together (a sharp ridge, inverted)
    SnakeHook,  // pull the surface out after the mouse, for horns and tails (see beginSnake)
    Mask,     // paint where the other brushes can't reach (unpaint, inverted)
    MaskClear,   // a whole-mesh step: no mask anywhere
    MaskInvert,  // a whole-mesh step: masked and free swapped
    Remesh,      // a whole-mesh step: even triangles of a size (the stroke's radius)
    Pattern,     // press a repeating pattern in (see beginPattern); engrave, inverted
};

// The grey picture of a pattern: built-in ones are made (the same every time, and
// repeating seamlessly), a user's is their picture. Throws if a built-in name is unknown.
std::shared_ptr<const PictureData> patternImage(const SculptPattern& pattern);
// The built-in patterns' names, in the order to offer them.
const std::vector<std::string>& builtInPatterns();
// How deep a Pattern stroke presses (negative: engraved): a share of its repeat size.
double patternDepth(const SculptStroke& stroke);
// A stamp's brush radius: enough to reach its picture's corners (`tile` wide).
double stampRadius(const PictureData& image, double tile);

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

// A mesh being sculpted: its points move in place. With detail on (setDetail), long
// edges under the brush are split first, adding points and triangles at the end of the
// lists (a triangle that's split keeps its place as its first piece).
// Numbers the points (and triangles) so that ones near each other in space are near
// each other in the lists: the shape is unchanged. The Sculptor does this to its mesh.
void spatialOrder(Mesh& mesh);

class Sculptor {
public:
    explicit Sculptor(Mesh mesh);

    const Mesh& mesh() const { return m_mesh; }
    // Detail where the brush goes: edges under it longer than `finest` (or 8% of the
    // brush, whichever is more) are split before it acts, up to maxTriangles in all.
    // 0: off (the triangles never change).
    void setDetail(double finest, std::size_t maxTriangles = 4'000'000);
    // Goes up whenever triangles are added (or the mesh is replaced).
    std::uint64_t topologyVersion() const { return m_topology; }
    // Replaces the whole mesh (an undo that has to take back added triangles).
    void setMesh(Mesh mesh);
    // Where a ray first meets the surface (through a grid of the triangles: quick even
    // on millions).
    std::optional<Hit> hit(const Vec3& origin, const Vec3& direction) const;
    // One dab of a brush (not Grab). Returns the range of points changed, [first, last)
    // (empty if none; new points included); their normals are updated too.
    std::pair<std::uint32_t, std::uint32_t> apply(const Dab& dab, std::uint32_t nearTriangle);
    // The same dab, and its mirror images if `symmetry` has any. The images start from
    // the point nearest each mirrored centre.
    std::pair<std::uint32_t, std::uint32_t> apply(const Dab& dab, std::uint32_t nearTriangle,
                                                  const Symmetry& symmetry);
    // The point nearest `p` (a brute-force search).
    std::uint32_t nearestPoint(const Vec3& p) const;
    // A triangle at (or very near) a point on the surface, through the grid; where a dab
    // starts its walk.
    std::uint32_t triangleAt(const Vec3& p) const;
    // The nearest point of the surface to `p` (near the triangle triangleAt finds). Strokes
    // are played from here, so they stay on a surface that has grown or shrunk.
    Vec3 onSurface(const Vec3& p) const;

    // Grab: on press, the points under the brush (and their mirror images) are taken
    // with how much each follows; each move puts them at where they were + offset x
    // weight (the offset mirrored for the images).
    void beginGrab(const Vec3& centre, double radius, std::uint32_t nearTriangle, const Symmetry& symmetry);
    std::pair<std::uint32_t, std::uint32_t> moveGrab(const Vec3& offset);
    void endGrab();
    // Snake hook: on press, the tip is the point under the brush; each move pulls the
    // surface round the tip after it to `to` (and the mirror images likewise), adding
    // detail as it stretches.
    void beginSnake(const Vec3& centre, double radius, double strength, std::uint32_t nearTriangle,
                    const Symmetry& symmetry);
    std::pair<std::uint32_t, std::uint32_t> moveSnake(const Vec3& to);
    void endSnake();

    // The mask (the mesh's shade): 1 where brushes can't reach, 0 where they act fully.
    // Both return the points changed.
    // Pattern: dabs between beginPattern and endPattern press `image` in (light parts
    // highest), repeating every `tile` mm across the surface, up to `depth` mm (below the
    // surface if negative), turned `turn` degrees round the facing. Going over the same
    // place again doesn't press it deeper. `once`: a stamp, the picture pressed once,
    // `tile` mm wide, centred on the first dab (nothing outside it).
    void beginPattern(std::shared_ptr<const PictureData> image, double tile, double depth, double turn = 0.0,
                      bool once = false);
    void endPattern();

    std::pair<std::uint32_t, std::uint32_t> clearMask();
    std::pair<std::uint32_t, std::uint32_t> invertMask();
    // The whole mesh made of even triangles about `edge` across (see remesh::remeshed);
    // the mask comes along.
    void remesh(double edge);
    float maskAt(std::uint32_t point) const { return m_mesh.shade.empty() ? 0.0f : m_mesh.shade[point]; }

    // Puts every point back as given (an undo); normals recomputed.
    void setPositions(const std::vector<float>& positions);

    // What changed since the last call, for sending only that to the graphics card:
    // runs of points [first, last) (positions, normals and mask) and of triangles,
    // in order, each run a whole number of blocks of 256 (the last one cut at the end).
    // `everything`: the whole mesh was replaced or redone; send it all.
    struct Changes {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> points;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> triangles;
        bool everything = false;
    };
    Changes takeChanges();
    // Files the triangles moved since the last time in the grid that finds the surface
    // (done at the end of each stroke, and some on each dab once very many have moved):
    // all of them, or the `most` that moved first.
    void settle(std::size_t most = SIZE_MAX);

private:
    std::pair<std::uint32_t, std::uint32_t> normalsAround(const std::vector<std::uint32_t>& moved);
    void normalsOf(const std::vector<std::uint32_t>& points);
    std::vector<std::uint32_t> pointsWithin(const Vec3& centre, double radius, std::uint32_t nearTriangle);
    std::vector<std::uint32_t> pointsFrom(const Vec3& centre, double radius, const std::vector<std::uint32_t>& seeds);
    std::pair<std::uint32_t, std::uint32_t> applyFrom(const Dab& dab, const std::vector<std::uint32_t>& seeds);

    bool splitAround(const std::vector<std::uint32_t>& points, double maxEdge);
    void rebuild();
    std::array<std::int32_t, 6> cellsOf(std::uint32_t triangle) const;
    void gridInsert(std::uint32_t triangle);
    void gridRemove(std::uint32_t triangle);
    void gridUpdate(const std::vector<std::uint32_t>& movedPoints);

    struct Grabbed {
        std::uint32_t point;
        float weight;
        std::array<float, 3> from;
        std::uint8_t image;
    };
    std::vector<Grabbed> m_grab;
    Symmetry m_grabSymmetry;

    struct SnakeImage {
        Vec3 tip;
        std::vector<std::uint32_t> seeds;  // the points pulled last time, to walk out from
    };
    struct PatternLayer {
        std::shared_ptr<const PictureData> image;
        double tile = 1.0;
        double depth = 0.0;
        double turn = 0.0;
        bool once = false;
        std::array<double, 3> origin{0, 0, 0};  // a stamp's middle
        bool framed = false;  // u, v set (from the first dab's facing)
        std::array<double, 3> u{1, 0, 0}, v{0, 1, 0};
        struct Base {
            std::array<double, 3> at;      // where the point was before the pattern
            std::array<double, 3> facing;  // its normal then
            float done = 0.0f;             // the most of the pattern it has had (0..1)
        };
        std::unordered_map<std::uint32_t, Base> base;
    };
    std::optional<PatternLayer> m_pattern;
    Symmetry m_dabSymmetry;           // of the dab being applied, and which image of it
    std::size_t m_dabImage = 0;
    std::vector<SnakeImage> m_snake;
    Symmetry m_snakeSymmetry;
    double m_snakeRadius = 0.0;
    double m_snakeFollow = 1.0;

    Mesh m_mesh;
    std::vector<std::vector<std::uint32_t>> m_vt;  // the triangles round each point
    std::vector<std::uint32_t> m_mark;             // for walks: the walk number that last reached it
    std::vector<std::uint32_t> m_triMark;
    std::uint32_t m_walk = 0;
    double m_detail = 0.0;
    std::size_t m_maxTriangles = 4'000'000;
    std::uint64_t m_topology = 0;
    // What's changed since takeChanges, in blocks of 256 points / triangles.
    void noteChanged(const std::vector<std::uint32_t>& points);
    void noteTriangle(std::uint32_t triangle);
    std::vector<char> m_pointBlockDirty, m_triBlockDirty;
    std::vector<std::uint32_t> m_pointBlocks, m_triBlocks;
    bool m_everything = true;
    // Triangles moved but not yet refiled in the grid: hit() and triangleAt() test them
    // all directly as well. Refiling every moved triangle on every dab was most of a
    // big brush's time.
    std::vector<std::uint32_t> m_stale;
    std::vector<char> m_isStale;
    // The grid: cells m_cell across, each listing the triangles whose box reaches it.
    double m_cell = 1.0;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_cells;
    std::vector<std::array<std::int32_t, 6>> m_triCells;
    std::array<std::int32_t, 3> m_cellLo{0, 0, 0}, m_cellHi{0, 0, 0};
};

// ---- Sculpt steps: strokes kept and played again ----

// How many triangles each starting detail aims for (0 Low, 1 Medium, 2 High).
std::size_t trianglesFor(int detail);
// The mesh a Sculpt step starts from: a shape's surface (any triangles) with its corners
// shared, split evenly to the detail. The same on every replay.
Mesh startingMesh(const Mesh& surface, int detail);
// The mirror planes for a stroke, through the middle of the starting mesh.
Symmetry symmetryFor(const SculptStroke& stroke, const Mesh& start);
// One recorded stroke played on a sculptor (as when it was made).
void replay(Sculptor& sculptor, const SculptStroke& stroke, const Mesh& start,
            const std::vector<SculptPattern>& patterns = {});
// Every stroke played on the starting mesh: the sculpted surface.
Mesh replayed(const Mesh& start, const SculptFinish& finish);

}  // namespace forge::sculpt
