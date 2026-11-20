// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 55: thin-wall and overhang check.
#include "forge/core/MeshKernel.h"
#include "forge/core/PrintCheck.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

// A box from `lo` to `hi`, faces outward, added to `m`.
void addBox(Mesh& m, const Vec3& lo, const Vec3& hi)
{
    const auto base = std::uint32_t(m.vertexCount());
    for (int k = 0; k < 8; ++k) {
        m.positions.push_back(float((k & 1) ? hi[0] : lo[0]));
        m.positions.push_back(float((k & 2) ? hi[1] : lo[1]));
        m.positions.push_back(float((k & 4) ? hi[2] : lo[2]));
    }
    const std::uint32_t faces[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6},   // bottom, top
                                        {0, 1, 4}, {1, 5, 4}, {2, 6, 3}, {3, 6, 7},   // front, back
                                        {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};  // left, right
    for (const auto& f : faces)
        for (int k = 0; k < 3; ++k)
            m.indices.push_back(base + f[k]);
}

Mesh box(const Vec3& lo, const Vec3& hi)
{
    Mesh m;
    addBox(m, lo, hi);
    return m;
}

}  // namespace

TEST_CASE("Print check: a thin slab is thin on its broad faces only", "[printcheck]")
{
    const Mesh slab = box({0, 0, 0}, {10, 10, 0.5});
    const auto thick = printcheck::faceThickness(slab);
    REQUIRE(thick.size() == 12);
    CHECK_THAT(thick[0], WithinAbs(0.5, 1e-4));  // bottom: through to the top
    CHECK_THAT(thick[2], WithinAbs(0.5, 1e-4));  // top
    CHECK(thick[4] == printcheck::kNoWall);      // the edges: 10 mm across, past kMaxWall

    printcheck::Settings s;
    s.overhangs = false;
    Mesh shown;
    auto r = printcheck::classify(slab, thick, Transform::identity(), s, &shown);
    CHECK_THAT(r.area, WithinAbs(2 * 100 + 4 * 5, 1e-3));
    CHECK_THAT(r.thinArea, WithinAbs(200, 1e-3));
    CHECK_THAT(r.thinnest, WithinAbs(0.5, 1e-4));
    REQUIRE(shown.vertexCount() == 36);
    REQUIRE(shown.shade.size() == 36);
    CHECK(shown.shade[0] == printcheck::kShadeThin);
    CHECK(shown.shade[3 * 4] == printcheck::kShadeOk);

    s.minWall = 0.4;
    r = printcheck::classify(slab, thick, Transform::identity(), s);
    CHECK(r.thinArea == 0.0);
    s.thin = false;
    s.minWall = 0.8;
    r = printcheck::classify(slab, thick, Transform::identity(), s);
    CHECK(r.thinArea == 0.0);
    CHECK_THAT(r.thinnest, WithinAbs(0.5, 1e-4));  // still found
}

TEST_CASE("Print check: a hollow box's walls", "[printcheck]")
{
    const MeshSolid outer = meshkernel::fromTriangles(box({0, 0, 0}, {20, 20, 20}).positions,
                                                      box({0, 0, 0}, {20, 20, 20}).indices);
    const Mesh hole = box({0.6, 0.6, 0.6}, {19.4, 19.4, 19.4});
    const MeshSolid inner = meshkernel::fromTriangles(hole.positions, hole.indices);
    const Mesh hollow = meshkernel::displayMesh(meshkernel::subtracted(outer, {inner}));
    const auto thick = printcheck::faceThickness(hollow);
    printcheck::Settings s;
    s.overhangs = false;
    const auto r = printcheck::classify(hollow, thick, Transform::identity(), s);
    CHECK_THAT(r.thinnest, WithinAbs(0.6, 1e-3));
    // Every face, inside and out, is 0.6 mm from the other side.
    CHECK_THAT(r.thinArea, WithinAbs(r.area, 1e-2));
}

TEST_CASE("Print check: overhangs, the way it sits on the plate", "[printcheck]")
{
    printcheck::Settings s;
    s.thin = false;
    // A cube sitting on its bottom: nothing hangs over.
    const Mesh cube = box({0, 0, 0}, {10, 10, 10});
    auto r = printcheck::classify(cube, {}, Transform::translation({0, 0, 3}), s);
    CHECK(r.overhangArea == 0.0);

    // A cube and, beside it, a ledge held up off the plate: the ledge's underside hangs.
    Mesh two = box({0, 0, 0}, {10, 10, 10});
    addBox(two, {10, 0, 8}, {14, 10, 10});
    r = printcheck::classify(two, {}, Transform::identity(), s);
    CHECK_THAT(r.overhangArea, WithinAbs(40, 1e-3));

    // Tipped 20 degrees: the bottom leans out 70 degrees from upright, one side 20.
    const Transform tipped = Transform::rotationXYZ(20, 0, 0);
    r = printcheck::classify(cube, {}, tipped, s);
    CHECK_THAT(r.overhangArea, WithinAbs(100, 1e-3));
    s.angle = 15;
    r = printcheck::classify(cube, {}, tipped, s);
    CHECK_THAT(r.overhangArea, WithinAbs(200, 1e-3));
    s.overhangs = false;
    r = printcheck::classify(cube, {}, tipped, s);
    CHECK(r.overhangArea == 0.0);
}

TEST_CASE("Print check: many parts at once (split over the cores)", "[printcheck]")
{
    Mesh many;
    for (int k = 0; k < 2000; ++k) {
        const double x = (k % 50) * 3.0, y = (k / 50) * 3.0;
        addBox(many, {x, y, 0}, {x + 2, y + 2, (k % 2) ? 0.3 : 1.5});
    }
    const auto thick = printcheck::faceThickness(many);
    REQUIRE(thick.size() == 24000);
    printcheck::Settings s;
    s.overhangs = false;
    const auto r = printcheck::classify(many, thick, Transform::identity(), s);
    CHECK_THAT(r.thinnest, WithinAbs(0.3, 1e-4));
    // The thin ones' tops and bottoms (their sides are 2 mm across).
    CHECK_THAT(r.thinArea, WithinAbs(1000 * 8.0, 1e-2));
}
