// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 4b: handles on parts inside combined shapes; resizing combined shapes.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Scale.h"
#include "forge/core/Split.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

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

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, Placement p = {})
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement = p;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId merge(CommandStack& stack, Document& doc, std::vector<ObjectId> parts)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Merge");
    r.boolean = BooleanSpec{BooleanOp::Merge, std::move(parts)};
    const auto c = doc.worldBounds().center();
    r.boolean->pivot = {c[0], c[1], c[2]};
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

void apply(CommandStack& stack, Document& doc, const std::vector<Body>& after)
{
    std::vector<Body> before;
    for (const Body& b : after)
        before.push_back(*doc.find(b.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
}

void requireBounds(const Bounds& b, Vec3 lo, Vec3 hi, double tol = 1e-3)
{
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(b.min[i], WithinAbs(lo[i], tol));
        CHECK_THAT(b.max[i], WithinAbs(hi[i], tol));
    }
}

}  // namespace

TEST_CASE("Resizing a combined shape scales its parts", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    Placement up;
    up.z = 20;
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 15}, up);
    const ObjectId r = merge(stack, doc, {box, peg});
    const double v0 = volumeOf(doc.shape(r));

    SECTION("evenly, from the base")
    {
        apply(stack, doc, scaledTree(doc, r, {2, 2, 2}, {0, 0, 0}));
        requireBounds(doc.worldBounds(r), {-20, -20, 0}, {20, 20, 70});
        CHECK_THAT(volumeOf(doc.shape(r)), WithinRel(v0 * 8, 1e-9));
        stack.undo();
        requireBounds(doc.worldBounds(r), {-10, -10, 0}, {10, 10, 35});
        stack.redo();
        CHECK(std::get<CylinderSpec>(doc.find(peg)->spec) == CylinderSpec{10, 30});
    }
    SECTION("taller only")
    {
        apply(stack, doc, scaledTree(doc, r, {1, 1, 2}, {0, 0, 0}));
        requireBounds(doc.worldBounds(r), {-10, -10, 0}, {10, 10, 70});
        CHECK_THAT(volumeOf(doc.shape(r)), WithinRel(v0 * 2, 1e-9));
    }
    SECTION("wider only, keeping the left side")
    {
        apply(stack, doc, scaledTree(doc, r, {1.5, 1, 1}, {-10, 0, 0}));
        requireBounds(doc.worldBounds(r), {-10, -10, 0}, {20, 10, 35});
        CHECK(std::get<CylinderSpec>(doc.find(peg)->spec) == CylinderSpec{7.5, 5, 15});  // oval now
    }
}

TEST_CASE("Parts turned at an angle can only be scaled evenly", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    Placement tilted;
    tilted.rotationZ = 45;
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 30}, tilted);
    const ObjectId r = merge(stack, doc, {a, b});
    CHECK_THROWS_AS(scaledTree(doc, r, {2, 1, 1}, {0, 0, 0}), NotScalable);
    CHECK_NOTHROW(scaledTree(doc, r, {1, 1, 2}, {0, 0, 0}));  // turned about Z: taller is fine
    Placement leaning;
    leaning.rotationX = 30;
    stack.execute(std::make_unique<SetPlacementCommand>(b, leaning, false));
    CHECK_THROWS_AS(scaledTree(doc, r, {1, 1, 2}, {0, 0, 0}), NotScalable);
    CHECK_NOTHROW(scaledTree(doc, r, {2, 2, 2}, {0, 0, 0}));

    // A quarter turn is fine: X and Y swap.
    Document doc2;
    CommandStack stack2(doc2);
    const ObjectId c = add(stack2, doc2, BoxSpec{20, 20, 20});
    Placement quarter;
    quarter.rotationZ = 90;
    const ObjectId d = add(stack2, doc2, BoxSpec{10, 30, 5}, quarter);  // 30 long along plate X
    const ObjectId r2 = merge(stack2, doc2, {c, d});
    apply(stack2, doc2, scaledTree(doc2, r2, {2, 1, 1}, {0, 0, 0}));
    CHECK(std::get<BoxSpec>(doc2.find(d)->spec) == BoxSpec{10, 60, 5});
}

TEST_CASE("Torus parts stretch evenly across the ring only", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{40, 40, 4});
    const ObjectId t = add(stack, doc, TorusSpec{12, 3});
    const ObjectId r = merge(stack, doc, {a, t});
    CHECK_THROWS_AS(scaledTree(doc, r, {2, 1, 1}, {0, 0, 0}), NotScalable);
    apply(stack, doc, scaledTree(doc, r, {1, 1, 3}, {0, 0, 0}));
    CHECK(std::get<TorusSpec>(doc.find(t)->spec) == TorusSpec{12, 12, 3, 9});
}

TEST_CASE("A split half stretches with its cut", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    const auto half = splitBody(stack, doc, box, SplitPlane{{0, 0, 5}, {0, 0, 1}}, SplitKeep::Front).front();
    requireBounds(doc.worldBounds(half), {-10, -10, 5}, {10, 10, 20});
    apply(stack, doc, scaledTree(doc, half, {1, 1, 2}, {0, 0, 5}));  // anchored at the cut
    requireBounds(doc.worldBounds(half), {-10, -10, 5}, {10, 10, 35});
}

TEST_CASE("A lone primitive scales about an anchor", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    Placement p;
    p.x = 5;
    p.rotationZ = 30;
    const ObjectId cyl = add(stack, doc, CylinderSpec{4, 10}, p);
    apply(stack, doc, scaledTree(doc, cyl, {2, 2, 2}, {0, 0, 0}));
    CHECK(std::get<CylinderSpec>(doc.find(cyl)->spec) == CylinderSpec{8, 20});
    CHECK_THAT(doc.worldBounds(cyl).min[2], WithinAbs(0, 1e-4));
    CHECK_THAT(doc.worldBounds(cyl).center()[0], WithinAbs(5, 1e-4));
}

TEST_CASE("Moving and turning a part inside a turned shape", "[handles]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{40, 40, 10});
    Placement off;
    off.x = 10;
    off.z = 10;
    const ObjectId b = add(stack, doc, BoxSpec{10, 4, 10}, off);
    const ObjectId r = merge(stack, doc, {a, b});
    Placement turned = doc.find(r)->placement;
    turned.rotationZ = 90;
    stack.execute(std::make_unique<SetPlacementCommand>(r, turned, false));
    const Transform parent = doc.worldTransform(r);

    // Move the part +5 mm along the plate's X.
    const Bounds before = doc.worldBounds(b);
    stack.execute(std::make_unique<SetPlacementCommand>(b, movedOnPlate(*doc.find(b), {5, 0, 0}, parent), false));
    const Bounds after = doc.worldBounds(b);
    CHECK_THAT(after.min[0] - before.min[0], WithinAbs(5, 1e-4));
    CHECK_THAT(after.min[1] - before.min[1], WithinAbs(0, 1e-4));

    // Turn it a quarter about the plate's Z through its own middle: its middle stays.
    const Vec3 centre = doc.visibleCentre(b);
    const auto mid = doc.worldBounds(b).center();
    stack.execute(std::make_unique<SetPlacementCommand>(
        b, rotatedAboutPlateAxis(*doc.find(b), 2, 90, centre, parent), false));
    const Bounds turnedB = doc.worldBounds(b);
    CHECK_THAT(turnedB.center()[0], WithinAbs(mid[0], 1e-4));
    CHECK_THAT(turnedB.center()[1], WithinAbs(mid[1], 1e-4));
    const auto sz = turnedB.size();
    CHECK_THAT(sz[0], WithinAbs(before.size()[1], 1e-3));  // its long side swapped direction
}
