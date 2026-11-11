// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Picking.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <memory>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

double volumeOf(const TopoDS_Shape& s)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

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

ObjectId combine(CommandStack& stack, Document& doc, BooleanOp op, std::vector<ObjectId> parts)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(op));
    BooleanSpec spec;
    spec.op = op;
    spec.operands = std::move(parts);
    const Bounds b = doc.worldBounds();  // fine for these tests: everything is in the result
    const auto c = b.center();
    spec.pivot = {c[0], c[1], c[2]};
    r.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

void requireSameBounds(const Bounds& a, const Bounds& b, double tol = 1e-3)
{
    REQUIRE(a.valid);
    REQUIRE(b.valid);
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(a.min[i], WithinAbs(b.min[i], tol));
        CHECK_THAT(a.max[i], WithinAbs(b.max[i], tol));
    }
}

}  // namespace

TEST_CASE("subtract cuts a hole: box minus a cylinder through it", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 30}, 0, 0, -10);
    const ObjectId cut = combine(stack, doc, BooleanOp::Subtract, {box, peg});

    CHECK_THAT(volumeOf(doc.shape(cut)), WithinRel(40.0 * 40 * 10 - std::numbers::pi * 25 * 10, 1e-6));
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{cut});  // the parts left the plate
    CHECK(doc.parentOf(box) == cut);
    CHECK(doc.parentOf(peg) == cut);
    CHECK(stack.undoLabel() == "Subtract");
}

TEST_CASE("merge and intersect volumes are right", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    // Two 20 mm cubes overlapping by half along X.
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);

    SECTION("merge")
    {
        const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, b});
        CHECK_THAT(volumeOf(doc.shape(m)), WithinRel(30.0 * 20 * 20, 1e-6));
    }
    SECTION("intersect")
    {
        const ObjectId i = combine(stack, doc, BooleanOp::Intersect, {a, b});
        CHECK_THAT(volumeOf(doc.shape(i)), WithinRel(10.0 * 20 * 20, 1e-6));
    }
}

TEST_CASE("intersecting shapes that do not touch gives nothing", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{10, 10, 10});
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 10}, 100);
    CHECK(isEmptySolid(doc.previewCombine(BooleanOp::Intersect, {a, b})));
    CHECK_FALSE(isEmptySolid(doc.previewCombine(BooleanOp::Merge, {a, b})));
}

TEST_CASE("editing a part rebuilds the result", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 30}, 0, 0, -10);
    const ObjectId cut = combine(stack, doc, BooleanOp::Subtract, {box, peg});
    const auto before = doc.mesh(cut);

    stack.execute(std::make_unique<SetSpecCommand>(peg, CylinderSpec{8, 30}));  // bigger hole
    CHECK(doc.mesh(cut) != before);
    CHECK_THAT(volumeOf(doc.shape(cut)), WithinRel(16000.0 - std::numbers::pi * 64 * 10, 1e-6));

    stack.execute(std::make_unique<SetPlacementCommand>(peg, Placement{100, 0, -10, 0}, false));  // hole moved off
    CHECK_THAT(volumeOf(doc.shape(cut)), WithinRel(16000.0, 1e-6));

    stack.undo();
    stack.undo();
    CHECK_THAT(volumeOf(doc.shape(cut)), WithinRel(16000.0 - std::numbers::pi * 25 * 10, 1e-6));
}

TEST_CASE("moving a result does not rebuild it, and its parts move with it", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);
    const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, b});
    const auto mesh = doc.mesh(m);
    const Bounds partBefore = doc.worldBounds(a);

    Placement p;
    p.y = 50;
    stack.execute(std::make_unique<SetPlacementCommand>(m, p, false));
    CHECK(doc.mesh(m) == mesh);
    CHECK_THAT(doc.worldBounds(a).min[1], WithinAbs(partBefore.min[1] + 50, 1e-3));
}

TEST_CASE("separate leaves every part where it appeared", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{30, 10, 10});
    const ObjectId b = add(stack, doc, CylinderSpec{6, 4, 25}, 12, 0, 0);
    const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, b});

    // Move and tilt the whole result, so separating has real work to do.
    Placement p;
    p.x = 40;
    p.z = 5;
    p.rotationX = 30;
    p.rotationZ = 60;
    stack.execute(std::make_unique<SetPlacementCommand>(m, p, false));
    const Bounds aWorld = doc.worldBounds(a), bWorld = doc.worldBounds(b);

    stack.execute(std::make_unique<SeparateCommand>(m));
    CHECK(doc.find(m) == nullptr);
    CHECK(doc.isTopLevel(a));
    CHECK(doc.isTopLevel(b));
    requireSameBounds(doc.worldBounds(a), aWorld);
    requireSameBounds(doc.worldBounds(b), bWorld);

    stack.undo();  // back together, parts back in the result's frame
    REQUIRE(doc.find(m));
    CHECK(doc.parentOf(a) == m);
    requireSameBounds(doc.worldBounds(a), aWorld);
}

TEST_CASE("undo of combine puts the parts back on the plate", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);
    const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, b});
    stack.undo();
    CHECK(doc.find(m) == nullptr);
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{a, b});
    stack.redo();
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{m});
}

TEST_CASE("results can be combined again (nesting)", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);
    const ObjectId m = combine(stack, doc, BooleanOp::Merge, {a, b});
    const ObjectId hole = add(stack, doc, CylinderSpec{3, 50}, 5, 0, -10);
    const ObjectId cut = combine(stack, doc, BooleanOp::Subtract, {m, hole});

    CHECK(doc.withDescendants(cut) == std::vector<ObjectId>{cut, m, a, b, hole});
    CHECK_THAT(volumeOf(doc.shape(cut)), WithinRel(12000.0 - std::numbers::pi * 9 * 20, 1e-6));

    // A part deep inside still rebuilds the top.
    stack.execute(std::make_unique<SetSpecCommand>(b, BoxSpec{20, 20, 30}));
    CHECK(volumeOf(doc.shape(cut)) > 12000.0);

    // The inner result can't be separated while it is a part.
    CHECK_THROWS(stack.execute(std::make_unique<SeparateCommand>(m)));
}

TEST_CASE("a part inside a result cannot be deleted on its own", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);
    combine(stack, doc, BooleanOp::Merge, {a, b});
    CHECK_THROWS_AS(stack.execute(std::make_unique<RemoveBodyCommand>(a)), std::invalid_argument);
}

TEST_CASE("bad combines are refused", "[combine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10);

    Body one;
    one.id = doc.allocateId();
    one.boolean = BooleanSpec{BooleanOp::Merge, {a}, {}};
    CHECK_THROWS(stack.execute(std::make_unique<CombineCommand>(one)));

    Body twice;
    twice.id = doc.allocateId();
    twice.boolean = BooleanSpec{BooleanOp::Merge, {a, a}, {}};
    CHECK_THROWS(stack.execute(std::make_unique<CombineCommand>(twice)));

    combine(stack, doc, BooleanOp::Merge, {a, b});
    const ObjectId c = add(stack, doc, BoxSpec{5, 5, 5}, 50);
    Body reuse;
    reuse.id = doc.allocateId();
    reuse.boolean = BooleanSpec{BooleanOp::Merge, {a, c}, {}};  // a is already used
    CHECK_THROWS(stack.execute(std::make_unique<CombineCommand>(reuse)));
}

TEST_CASE("picking hits a result, never a hidden part", "[combine][picking]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 30}, 0, 0, -10);
    const ObjectId cut = combine(stack, doc, BooleanOp::Subtract, {box, peg});

    const auto onSolid = pick(doc, Ray{{15, 15, 100}, {0, 0, -1}});
    REQUIRE(onSolid);
    CHECK(onSolid->id == cut);
    CHECK_FALSE(pick(doc, Ray{{0, 0, 100}, {0, 0, -1}}));  // straight down the hole
}

TEST_CASE("combine results survive saving and opening", "[combine][file]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 30}, 0, 0, -10);
    const ObjectId cut = combine(stack, doc, BooleanOp::Subtract, {box, peg});
    const double volume = volumeOf(doc.shape(cut));

    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / ("forge3d-combine-" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const auto path = dir / "cut.f3d";
    file::save(doc, path, "test");

    Document reopened;
    reopened.replaceAll(file::load(path));
    std::filesystem::remove_all(dir);

    CHECK(reopened.topLevelIds() == std::vector<ObjectId>{cut});
    CHECK(reopened.parentOf(peg) == cut);
    CHECK(*reopened.find(cut) == *doc.find(cut));
    CHECK_THAT(volumeOf(reopened.shape(cut)), WithinRel(volume, 1e-9));
}

TEST_CASE("files with broken combine references are refused", "[combine][file]")
{
    const auto bodies = file::fromJson(R"({"format":"forge3d-document","formatVersion":4,"units":"mm","bodies":[
        {"id":1,"name":"Box","type":"box","parameters":{"width":1,"depth":1,"height":1},
         "placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}},
        {"id":2,"name":"Merge","type":"combine","operation":"merge","operands":[1,9],"pivot":[0,0,0],
         "placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}}]})");
    Document doc;
    CHECK_THROWS(doc.replaceAll(bodies));  // part 9 does not exist
    CHECK(doc.empty());
}
