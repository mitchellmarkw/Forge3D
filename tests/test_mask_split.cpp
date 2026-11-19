// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 48: Split by Mask.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Shape");
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

double meshVolume(const Mesh& m)
{
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[3 * m.indices[t]];
        const float* b = &m.positions[3 * m.indices[t + 1]];
        const float* c = &m.positions[3 * m.indices[t + 2]];
        v += (a[0] * (double(b[1]) * c[2] - double(b[2]) * c[1]) - a[1] * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
              a[2] * (double(b[0]) * c[1] - double(b[1]) * c[0])) /
             6.0;
    }
    return v;
}

// A stroke of dabs from a to b.
void stroke(std::vector<MaskDab>& dabs, const Vec3& a, const Vec3& b, double radius, bool erase = false)
{
    const double len = std::hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    const int n = std::max(1, int(len / (radius / 4.0)));
    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        dabs.push_back({{a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}, radius, erase});
    }
}

}  // namespace

TEST_CASE("Mask: the last dab covering a point decides", "[mask]")
{
    std::vector<MaskDab> dabs{{{0, 0, 0}, 2.0, false}, {{1, 0, 0}, 1.0, true}};
    mask::Field f(dabs);
    CHECK(f.masked({-1.5, 0, 0}));
    CHECK(!f.masked({1.2, 0, 0}));  // erased afterwards
    CHECK(!f.masked({5, 0, 0}));
    dabs.push_back({{1.2, 0, 0}, 0.3, false});  // painted again over the erased part
    mask::Field g(dabs);
    CHECK(g.masked({1.2, 0, 0}));
    CHECK(!g.masked({0.8, 0, 0}));
    CHECK(g.nearEdge({2.05, 0, 0}, 0.1));
    CHECK(!g.nearEdge({-1.0, 0, 0}, 0.1));
}

TEST_CASE("Mask: a box's corner cut off; the pieces fit back together", "[mask]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(doc.meshSolid(box), pos, idx);
    // A ball of paint over the top corner.
    const std::vector<MaskDab> dabs{{{10, 10, 20}, 8.0, false}};
    const mask::Pieces p = mask::split(pos, idx, dabs);
    CHECK(p.caps == 1);
    // Both closed solids, and together the whole box: the cap is shared.
    const double a = meshkernel::volume(meshkernel::fromTriangles(p.masked.positions, p.masked.indices));
    const double b = meshkernel::volume(meshkernel::fromTriangles(p.rest.positions, p.rest.indices));
    CHECK(a > 10.0);
    CHECK(b > 7000.0);
    CHECK_THAT(a + b, WithinRel(8000.0, 1e-6));
    CHECK_THAT(meshVolume(p.masked) + meshVolume(p.rest), WithinRel(8000.0, 1e-6));
    // The cut follows the ball: the corner piece is about an eighth of the ball, less what
    // the cap bulges (a membrane spanning the curved edge, not the ball's own surface).
    const double eighth = 4.0 / 3.0 * std::numbers::pi * 512.0 / 8.0;
    CHECK(a < eighth);
    CHECK(a > 0.5 * eighth);

    CHECK_THROWS_AS(mask::split(pos, idx, {{{100, 100, 100}, 1.0, false}}), std::invalid_argument);  // nothing
    CHECK_THROWS_AS(mask::split(pos, idx, {{{0, 0, 10}, 100.0, false}}), std::invalid_argument);     // everything
}

TEST_CASE("Mask: a band round a cylinder: two cuts, three pieces' worth", "[mask]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId cyl = add(stack, doc, CylinderSpec{5, 30});
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(doc.meshSolid(cyl), pos, idx);
    // Painted round its middle, a band about 6 mm wide; then a little erased and painted again.
    std::vector<MaskDab> dabs;
    for (int i = 0; i <= 64; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 64.0;
        dabs.push_back({{5 * std::cos(a), 5 * std::sin(a), 15}, 3.0, false});
    }
    stroke(dabs, {5, 0, 12}, {5, 0, 18}, 1.0, true);
    stroke(dabs, {5, 0, 12}, {5, 0, 18}, 1.5, false);
    const mask::Pieces p = mask::split(pos, idx, dabs);
    CHECK(p.caps == 2);
    const double total = std::numbers::pi * 25.0 * 30.0;
    const double a = meshkernel::volume(meshkernel::fromTriangles(p.masked.positions, p.masked.indices));
    const double b = meshkernel::volume(meshkernel::fromTriangles(p.rest.positions, p.rest.indices));
    CHECK_THAT(a + b, WithinRel(total * (meshVolume(p.masked) + meshVolume(p.rest)) / total, 1e-6));
    // The band: about 6 mm of the 30 mm length (the polygon a little less than the circle).
    CHECK_THAT(a / (a + b), WithinAbs(0.2, 0.02));
}

TEST_CASE("Mask: Split by Mask on the plate; changed, saved, scaled", "[mask]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, 30, 0, 0);
    const double whole = volumeOf(doc, box);
    const std::vector<MaskDab> dabs{{{40, 10, 20}, 8.0, false}};
    CHECK_THROWS_AS(splitByMask(stack, doc, box, {{{0, 0, 100}, 1.0, false}}, SplitKeep::Both), std::invalid_argument);
    const auto made = splitByMask(stack, doc, box, dabs, SplitKeep::Both);
    REQUIRE(made.size() == 2);
    CHECK(doc.find(made[0])->name == "Shape 1 (masked)");
    CHECK(doc.find(made[1])->name == "Shape 1 (rest)");
    CHECK(stack.undoLabel() == "Split Shape 1 by mask");
    CHECK(history::describe(doc, made[0]) == "the masked part (1 dab)");
    const double a = volumeOf(doc, made[0]), b = volumeOf(doc, made[1]);
    CHECK_THAT(a + b, WithinRel(whole, 1e-6));
    // Where it was painted, as given.
    const auto back = maskDabs(doc, made[0]);
    REQUIRE(back.size() == 1);
    CHECK_THAT(back[0].centre[0], WithinAbs(40.0, 1e-9));

    // A bigger ball of paint: both pieces change together.
    const auto changed = withMask(doc, made[0], {{{40, 10, 20}, 12.0, false}});
    REQUIRE(changed.size() == 2);
    std::vector<Body> before;
    for (const Body& c : changed)
        before.push_back(*doc.find(c.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, changed, "Change mask"));
    CHECK(volumeOf(doc, made[0]) > a);
    CHECK_THAT(volumeOf(doc, made[0]) + volumeOf(doc, made[1]), WithinRel(whole, 1e-6));

    // Saved and read back.
    Document reread;
    reread.replaceAll(file::fromJson(file::toJson(doc, "test")));
    CHECK(reread.find(made[0])->boolean->maskSplit == doc.find(made[0])->boolean->maskSplit);
    CHECK_THAT(volumeOf(reread, made[1]), WithinRel(volumeOf(doc, made[1]), 1e-6));

    // Scaled evenly, the paint grows with it.
    const auto tree = scaledTree(doc, made[0], {2.0, 2.0, 2.0}, {30.0, 0.0, 0.0});
    REQUIRE(tree.back().boolean->maskSplit);
    CHECK_THAT(tree.back().boolean->maskSplit->dabs[0].radius, WithinAbs(24.0, 1e-9));
    CHECK_THAT(tree.back().boolean->maskSplit->dabs[0].centre[0], WithinAbs(50.0, 1e-9));

    stack.undo();
    stack.undo();
    CHECK(doc.find(box));
    CHECK(doc.isTopLevel(box));
    CHECK(!doc.find(made[0]));
}
