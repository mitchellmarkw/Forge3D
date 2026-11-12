// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 6: an object's history: steps, switching steps off, the rollback bar, and
// adding or deleting steps inside it.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <memory>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

constexpr double pi = std::numbers::pi;

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(doc.shape(id), props);
    return props.Mass();
}

// Picks the edges nearest these plate points of `target` (on the plate or inside a history).
EdgeFinish picks(const Document& doc, ObjectId target, std::vector<Vec3> near, double size)
{
    const TopoDS_Shape shape = pickableShape(doc, target);
    const auto found = edges::analyse(shape);
    EdgeFinish f;
    f.kind = EdgeFinishKind::Rounding;
    f.size = size;
    for (const Vec3& p : near) {
        int best = -1;
        double bestD = 1e300;
        for (std::size_t i = 0; i < found.edges.size(); ++i) {
            const Vec3& m = found.edges[i].mid;
            const double d = std::hypot(m[0] - p[0], m[1] - p[1], m[2] - p[2]);
            if (d < bestD) {
                bestD = d;
                best = int(i);
            }
        }
        f.edges.push_back(edges::refFor(shape, found, best, -1));
    }
    return f;
}

ObjectId combine(CommandStack& stack, Document& doc, BooleanOp op, std::vector<ObjectId> parts)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(op));
    r.boolean = BooleanSpec{op, std::move(parts)};
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

// Box (20 x 20 x 10) -> rounded front top edge R2 -> minus a cylinder through it.
struct Built {
    ObjectId box, rounded, cyl, cut;
};
Built build(CommandStack& stack, Document& doc)
{
    Built b;
    b.box = add(stack, doc, BoxSpec{20, 20, 10});
    Body r;
    r.id = doc.allocateId();
    r.name = "Rounded";
    r.boolean = edgesSpec(doc, b.box, picks(doc, b.box, {{0, -10, 10}}, 2.0));
    b.rounded = r.id;
    stack.execute(std::make_unique<CombineCommand>(r));
    b.cyl = add(stack, doc, CylinderSpec{3, 20}, 0, 0, -5);
    b.cut = combine(stack, doc, BooleanOp::Subtract, {b.rounded, b.cyl});
    return b;
}

const double kBox = 4000.0;
const double kRound = 20 * (4.0 - pi);  // one 20 mm edge at R2
const double kHole = pi * 9 * 10;

}  // namespace

TEST_CASE("An object's steps, oldest first, with descriptions", "[history]")
{
    Document doc;
    CommandStack stack(doc);
    const Built b = build(stack, doc);
    CHECK(history::steps(doc, b.cut) == std::vector<ObjectId>{b.box, b.rounded, b.cut});
    CHECK(history::objectOf(doc, b.box) == b.cut);
    CHECK(history::objectOf(doc, b.cyl) == b.cut);
    CHECK(history::barAt(doc, b.cut) == 3);
    CHECK(history::tip(doc, b.cut) == b.cut);
    CHECK(history::describe(doc, b.box) == "20 \xC3\x97 20 \xC3\x97 10 mm");
    CHECK(history::describe(doc, b.rounded) == "1 edge rounded R2");
    CHECK(history::describe(doc, b.cut) == "minus Cylinder 1");
    CHECK(history::describe(doc, b.cyl) == "R3 \xC3\x97 20 mm");
    CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound - kHole, 1e-4));
}

TEST_CASE("Switching steps off", "[history]")
{
    Document doc;
    CommandStack stack(doc);
    const Built b = build(stack, doc);
    const auto setOff = [&](ObjectId id, bool off) {
        Body before = *doc.find(id), after = before;
        after.suppressed = off;
        stack.execute(std::make_unique<EditBodyCommand>(before, after, "Suppress"));
    };

    SECTION("the rounding: the hole still goes through the plain box")
    {
        setOff(b.rounded, true);
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kHole, 1e-4));
        stack.undo();
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound - kHole, 1e-4));
    }
    SECTION("the subtract: no hole, the cylinder ignored")
    {
        setOff(b.cut, true);
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound, 1e-4));
        CHECK_THAT(doc.worldBounds(b.cut).min[2], WithinAbs(0.0, 1e-4));  // no cylinder poking out below
    }
    SECTION("a merge switched off keeps both parts")
    {
        const ObjectId a = add(stack, doc, BoxSpec{10, 10, 10}, 50);
        const ObjectId c = add(stack, doc, BoxSpec{10, 10, 10}, 55);
        const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, c});
        setOff(m, true);
        CHECK_THAT(doc.worldBounds(m).size()[0], WithinAbs(15.0, 1e-4));
    }
    SECTION("saved and loaded")
    {
        setOff(b.rounded, true);
        std::random_device rd;
        const fs::path dir = fs::temp_directory_path() / ("forge3d-history-" + std::to_string(rd()));
        fs::create_directories(dir);
        file::save(doc, dir / "h.f3d", "test");
        Document loaded;
        loaded.replaceAll(file::load(dir / "h.f3d"));
        CHECK(loaded.find(b.rounded)->suppressed);
        CHECK_THAT(volumeOf(loaded, b.cut), WithinRel(kBox - kHole, 1e-4));
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
}

TEST_CASE("The rollback bar", "[history]")
{
    Document doc;
    CommandStack stack(doc);
    const Built b = build(stack, doc);
    const auto rollTo = [&](std::size_t shown) {
        auto after = history::rolledTo(doc, b.cut, shown);
        if (after.empty())
            return;
        std::vector<Body> before;
        for (const Body& x : after)
            before.push_back(*doc.find(x.id));
        stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Roll back"));
    };

    rollTo(2);
    CHECK(history::barAt(doc, b.cut) == 2);
    CHECK(history::tip(doc, b.cut) == b.rounded);
    CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound, 1e-4));
    rollTo(1);
    CHECK(history::tip(doc, b.cut) == b.box);
    CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox, 1e-4));

    SECTION("rolling forward brings everything back")
    {
        rollTo(3);
        CHECK(history::barAt(doc, b.cut) == 3);
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound - kHole, 1e-4));
    }
    SECTION("a new step goes in at the bar")
    {
        // Round the back top edge of the box, at the bar (after the box, before the
        // first rounding), then roll forward: both edges rounded, still with the hole.
        Body r;
        r.id = doc.allocateId();
        r.name = "Rounded back";
        r.boolean = edgesSpec(doc, b.box, picks(doc, b.box, {{0, 10, 10}}, 2.0));
        const ObjectId added = r.id;
        stack.execute(std::make_unique<InsertStepCommand>(r, doc.parentOf(b.box)));
        CHECK(history::steps(doc, b.cut) == std::vector<ObjectId>{b.box, added, b.rounded, b.cut});
        CHECK(history::tip(doc, b.cut) == added);  // shown: box and the new step
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound, 1e-4));
        rollTo(4);
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - 2 * kRound - kHole, 1e-4));
        stack.undo();  // roll forward
        stack.undo();  // the insert
        CHECK(history::steps(doc, b.cut) == std::vector<ObjectId>{b.box, b.rounded, b.cut});
        CHECK_FALSE(doc.find(added));
    }
    SECTION("a later step that no longer works is left out, and says why")
    {
        // Chamfer the front-right top corner's vertical edge at the bar, then roll forward:
        // the rounding can't follow its edges round the new chamfer.
        Body c;
        c.id = doc.allocateId();
        c.name = "Chamfered";
        EdgeFinish f = picks(doc, b.box, {{10, -10, 5}}, 3.0);
        f.kind = EdgeFinishKind::Chamfer;
        c.boolean = edgesSpec(doc, b.box, f);
        stack.execute(std::make_unique<InsertStepCommand>(c, doc.parentOf(b.box)));
        rollTo(4);
        const double chamfer = 3 * 3 / 2.0 * 10;
        const double v = volumeOf(doc, b.cut);
        if (doc.problemWith(b.rounded)) {
            CHECK_THAT(v, WithinRel(kBox - chamfer - kHole, 1e-4));  // carried on without the rounding
            CHECK_FALSE(doc.problemWith(b.cut));
        } else {
            CHECK(v < kBox - chamfer - kHole);  // OpenCASCADE managed it after all
        }
        // Switching the chamfer off fixes it.
        Body before = *doc.find(c.id), after = before;
        after.suppressed = true;
        stack.execute(std::make_unique<EditBodyCommand>(before, after, "off"));
        CHECK_FALSE(doc.problemWith(b.rounded));
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound - kHole, 1e-4));
    }
    SECTION("works when the object has been moved")
    {
        rollTo(3);
        Placement p = doc.find(b.cut)->placement;
        p.x += 40;
        p.rotationZ = 90;
        stack.execute(std::make_unique<SetPlacementCommand>(b.cut, p, false));
        rollTo(1);
        // The box's top edge facing +X now (it was facing -Y before turning).
        const Bounds wb = doc.worldBounds(b.cut);
        const double cx = (wb.min[0] + wb.max[0]) / 2, cy = (wb.min[1] + wb.max[1]) / 2;
        Body r;
        r.id = doc.allocateId();
        r.name = "Rounded more";
        r.boolean = edgesSpec(doc, b.box, picks(doc, b.box, {{cx - 10, cy, 10}}, 2.0));
        stack.execute(std::make_unique<InsertStepCommand>(r, doc.parentOf(b.box)));
        rollTo(4);
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - 2 * kRound - kHole, 1e-4));
    }
}

TEST_CASE("Deleting a step and changing a combine", "[history]")
{
    Document doc;
    CommandStack stack(doc);
    const Built b = build(stack, doc);
    SECTION("delete the rounding")
    {
        stack.execute(std::make_unique<RemoveStepCommand>(b.rounded));
        CHECK(history::steps(doc, b.cut) == std::vector<ObjectId>{b.box, b.cut});
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kHole, 1e-4));
        stack.undo();
        CHECK(history::steps(doc, b.cut) == std::vector<ObjectId>{b.box, b.rounded, b.cut});
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kBox - kRound - kHole, 1e-4));
    }
    SECTION("delete an inner subtract: its tool goes back on the plate")
    {
        // Wrap the cut in another step so it isn't on top.
        const ObjectId other = add(stack, doc, BoxSpec{5, 5, 5}, 50);
        const ObjectId group = combine(stack, doc, BooleanOp::Group, {b.cut, other});
        stack.execute(std::make_unique<RemoveStepCommand>(b.cut));
        CHECK(doc.isTopLevel(b.cyl));
        CHECK(doc.parentOf(b.rounded) == group);
        stack.undo();
        CHECK(doc.parentOf(b.cyl) == b.cut);
        CHECK(doc.parentOf(b.cut) == group);
    }
    SECTION("subtract becomes intersect")
    {
        Body before = *doc.find(b.cut), after = before;
        after.boolean->op = BooleanOp::Intersect;
        stack.execute(std::make_unique<EditBodyCommand>(before, after, "Change to Intersect"));
        CHECK_THAT(volumeOf(doc, b.cut), WithinRel(kHole, 1e-4));
        CHECK(history::describe(doc, b.cut) == "overlap with Cylinder 1");
    }
}
