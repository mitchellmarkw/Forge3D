// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 99: lasso cut and lasso mask.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Lasso.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Primitives.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Split.h"
#include "forge/core/Tessellator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId addBox(CommandStack& stack, Document& doc, double size = 20.0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{size, size, size};
    b.name = "Box";
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id) { return meshkernel::volume(doc.meshSolid(id)); }

// A square drawn by hand (many points, a little wobble), seen from above, at height z.
lasso::Loop square(double x0, double y0, double x1, double y1, double z = 30.0)
{
    lasso::Loop loop;
    loop.along = {0, 0, -1};
    const double xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1};
    for (int side = 0; side < 4; ++side)
        for (int i = 0; i < 30; ++i) {
            const double t = i / 30.0;
            const double wob = 0.01 * std::sin(i * 1.7);
            loop.points.push_back({xs[side] + (xs[(side + 1) % 4] - xs[side]) * t + wob,
                                   ys[side] + (ys[(side + 1) % 4] - ys[side]) * t - wob, z});
        }
    return loop;
}

}  // namespace

TEST_CASE("Lasso: a hand-drawn loop is simplified and checked", "[lasso]")
{
    const lasso::Loop loop = square(-5, -5, 5, 5);
    const lasso::Loop simple = lasso::simplified(loop, 0.05);
    CHECK(simple.points.size() >= 4);
    CHECK(simple.points.size() <= 12);
    CHECK_FALSE(lasso::crossesItself(simple));

    lasso::Loop eight;
    eight.along = {0, 0, -1};
    for (const auto& p : std::vector<Vec3>{{0, 0, 0}, {10, 10, 0}, {10, 0, 0}, {0, 10, 0}})
        eight.points.push_back(p);
    CHECK(lasso::crossesItself(eight));
    Bounds b;
    b.expand(-10, -10, 0);
    b.expand(10, 10, 20);
    CHECK_THROWS_AS(lasso::cutter(eight, b), std::invalid_argument);

    const auto f = lasso::frameOf(loop);
    const auto outline = lasso::flat(loop, f);
    CHECK(lasso::insideFlat(outline, 0.0, 0.0));
    CHECK_FALSE(lasso::insideFlat(outline, 0.0, 6.0));
    CHECK_THAT(lasso::distanceFlat(outline, 0.0, 0.0), WithinAbs(5.0, 0.05));
}

TEST_CASE("Lasso cut: cut away, keep, or both", "[lasso]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc);  // 20 mm, x and y -10..10, z 0..20
    const lasso::Loop loop = square(-5, -5, 5, 5);

    SECTION("Cut away inside: a square hole right through")
    {
        const auto made = lasso::cut(stack, doc, box, loop, lasso::Keep::Outside);
        REQUIRE(made.size() == 1);
        CHECK(doc.topLevelIds().size() == 1);
        CHECK_THAT(volumeOf(doc, made[0]), WithinRel(8000.0 - 2000.0, 0.01));
        // The cutter is a sketch pad in the step: it can be changed like any sketch.
        const Body* step = doc.find(made[0]);
        REQUIRE(step->boolean->operands.size() == 2);
        CHECK(std::holds_alternative<PadSpec>(doc.find(step->boolean->operands[1])->spec));
        stack.undo();
        CHECK(doc.topLevelIds().size() == 1);
        CHECK_THAT(volumeOf(doc, box), WithinRel(8000.0, 1e-6));
    }
    SECTION("Keep inside")
    {
        const auto made = lasso::cut(stack, doc, box, loop, lasso::Keep::Inside);
        REQUIRE(made.size() == 1);
        CHECK_THAT(volumeOf(doc, made[0]), WithinRel(2000.0, 0.01));
    }
    SECTION("Both pieces, and saved")
    {
        const auto made = lasso::cut(stack, doc, box, loop, lasso::Keep::Both);
        REQUIRE(made.size() == 2);
        CHECK(doc.topLevelIds().size() == 2);
        CHECK(doc.find(made[0])->name == "Box (inside)");
        CHECK(doc.find(made[1])->name == "Box (outside)");
        CHECK_THAT(volumeOf(doc, made[0]), WithinRel(2000.0, 0.01));
        CHECK_THAT(volumeOf(doc, made[1]), WithinRel(6000.0, 0.01));
        const auto loaded = file::fromJson(file::toJson(doc, "test"));
        Document other;
        other.replaceAll(loaded);
        CHECK_THAT(volumeOf(other, made[1]), WithinRel(6000.0, 0.01));
        stack.undo();
        CHECK(doc.topLevelIds().size() == 1);
    }
    SECTION("A loop that misses, or takes everything, changes nothing")
    {
        CHECK_THROWS_AS(lasso::cut(stack, doc, box, square(30, 30, 40, 40), lasso::Keep::Outside),
                        std::invalid_argument);
        CHECK_THROWS_AS(lasso::cut(stack, doc, box, square(30, 30, 40, 40), lasso::Keep::Inside),
                        std::invalid_argument);
        CHECK_THROWS_AS(lasso::cut(stack, doc, box, square(-20, -20, 20, 20), lasso::Keep::Outside),
                        std::invalid_argument);
        CHECK(doc.bodies().size() == 1);
        stack.undo();  // only the box's adding was on the undo list
        CHECK(doc.bodies().empty());
    }
    SECTION("Seen from the side and at a slant")
    {
        lasso::Loop side;
        side.along = {0, 1, 0};  // from the front, looking back
        for (const Vec3& p : square(-5, 5, 5, 15).points)
            side.points.push_back({p[0], -40.0, p[1]});  // x -5..5, z 5..15
        const auto made = lasso::cut(stack, doc, box, side, lasso::Keep::Inside);
        CHECK_THAT(volumeOf(doc, made[0]), WithinRel(10.0 * 10.0 * 20.0, 0.01));
        stack.undo();
        lasso::Loop slant = square(-3, -3, 3, 3, 10.0);  // through the box's middle
        slant.along = {std::sqrt(0.5), 0, -std::sqrt(0.5)};
        const auto tilted = lasso::cut(stack, doc, box, slant, lasso::Keep::Inside);
        // A slanted bar through the box: more than nothing, less than straight down would be.
        const double v = volumeOf(doc, tilted[0]);
        // About 6 x 6 across, leaning 45 degrees: 36 x 20 / cos 45 less what leaves the sides.
        CHECK(v > 600.0);
        CHECK(v < 1100.0);
    }
}

TEST_CASE("Lasso mask: masks right through, in order with the brush", "[lasso][mask]")
{
    const lasso::Loop loop = square(-5, -5, 5, 5);
    std::vector<MaskDab> dabs{mask::lassoDab(loop.points, loop.along, false)};
    {
        const mask::Field field(dabs);
        CHECK(field.masked({0, 0, 0}));
        CHECK(field.masked({4, -4, 20}));  // the top and the bottom both
        CHECK_FALSE(field.masked({6, 0, 10}));
        CHECK(field.nearEdge({5.05, 0, 3}, 0.2));
        CHECK_FALSE(field.nearEdge({0, 0, 3}, 0.2));
    }
    // A brush dab after it rubs a bit out; a lasso after that paints it back.
    dabs.push_back({{0, 0, 20}, 2.0, true});
    {
        const mask::Field field(dabs);
        CHECK_FALSE(field.masked({0, 0, 20}));
        CHECK(field.masked({0, 0, 0}));
    }
    dabs.push_back(mask::lassoDab(square(-1, -1, 1, 1).points, {0, 0, -1}, false));
    CHECK(mask::Field(dabs).masked({0, 0, 20}));
    // Moved with the shape.
    const MaskDab moved = mask::movedDab(dabs.front(), Transform::translation({100, 0, 0}));
    CHECK(mask::Field({moved}).masked({100, 0, 5}));
    CHECK_FALSE(mask::Field({moved}).masked({0, 0, 5}));
}

TEST_CASE("Lasso mask: Split by Mask with a lasso, saved", "[lasso][mask]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc);
    // Round the right end, as you'd lasso a hand: everything right of x = 2, all round.
    const lasso::Loop loop = square(2, -15, 15, 15);
    const auto made =
        splitByMask(stack, doc, box, {mask::lassoDab(loop.points, loop.along, false)}, SplitKeep::Both);
    REQUIRE(made.size() == 2);
    CHECK_THAT(volumeOf(doc, made[0]), WithinRel(8.0 * 20.0 * 20.0, 0.02));
    CHECK(mask::describe(*doc.find(made[0])->boolean->maskSplit) == "the masked part (1 lasso)");
    CHECK_THAT(volumeOf(doc, made[0]) + volumeOf(doc, made[1]), WithinRel(8000.0, 0.01));
    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    CHECK(*other.find(made[0]) == *doc.find(made[0]));
    CHECK_THAT(volumeOf(other, made[0]), WithinRel(volumeOf(doc, made[0]), 1e-6));
}

TEST_CASE("Lasso mask: the sculpt mask", "[lasso][sculpt]")
{
    const Mesh start = sculpt::startingMesh(tessellate(primitives::buildShape(BoxSpec{20, 20, 20})), 0);
    sculpt::Sculptor sculptor(start);
    const lasso::Loop loop = square(2, -5, 8, 5);  // right of the middle
    SculptStroke stroke;
    stroke.brush = int(sculpt::Brush::MaskLasso);
    for (const Vec3& p : loop.points)
        stroke.dabs.push_back({float(p[0]), float(p[1]), float(p[2])});
    stroke.grab = {0, 0, -1};
    stroke.mirrorX = true;  // and the same on the left
    sculpt::replay(sculptor, stroke, start);
    const Mesh& m = sculptor.mesh();
    REQUIRE(m.shade.size() == m.vertexCount());
    int inside = 0, wrong = 0;
    for (std::size_t v = 0; v < m.vertexCount(); ++v) {
        const double x = m.positions[3 * v], y = m.positions[3 * v + 1];
        const bool in = std::abs(x) > 2.1 && std::abs(x) < 7.9 && std::abs(y) < 4.9;
        const bool out = std::abs(x) < 1.9 || std::abs(x) > 8.1 || std::abs(y) > 5.1;
        if (in) {
            ++inside;
            wrong += m.shade[v] != 1.0f;
        } else if (out) {
            wrong += m.shade[v] != 0.0f;
        }
    }
    CHECK(inside > 20);
    CHECK(wrong == 0);
    // Inverted, it takes the mask off again.
    stroke.invert = true;
    stroke.mirrorX = false;
    sculpt::replay(sculptor, stroke, start);
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        if (m.positions[3 * v] > 2.1 && m.positions[3 * v] < 7.9 && std::abs(m.positions[3 * v + 1]) < 4.9)
            CHECK(m.shade[v] == 0.0f);
}
