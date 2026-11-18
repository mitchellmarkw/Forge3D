// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 38: splitting for printing (keyed cuts, the smart split).
#include "forge/core/Body.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/PrintSplit.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

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

double vol(const Document& doc, ObjectId id)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(doc.shape(id), props);
    return props.Mass();
}

// A frustum of radii a, b and height h.
double frustum(double a, double b, double h)
{
    return pi * h / 3 * (a * a + a * b + b * b);
}

}  // namespace

TEST_CASE("Print split: a cut keyed with tapered pegs", "[printsplit]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{60, 40, 20});
    const SplitPlane level{{0, 0, 10}, {0, 0, 1}};
    CHECK_THAT(sectionArea(doc, box, level), WithinRel(2400.0, 1e-6));

    KeyOptions keys;
    const auto spots = keySpots(doc, box, level, keys);
    REQUIRE(spots.size() == 2);  // two keep the pieces from turning
    for (const KeySpot& s : spots) {
        CHECK_THAT(s.diameter, WithinAbs(5.0, 1e-9));
        CHECK_THAT(s.point[2], WithinAbs(10.0, 1e-6));
        // Well inside the cut, clear of its edges.
        CHECK(std::abs(s.point[0]) <= 30 - 2.5 - 0.2 - 1.2 + 1e-6);
        CHECK(std::abs(s.point[1]) <= 20 - 2.5 - 0.2 - 1.2 + 1e-6);
    }
    CHECK(std::hypot(spots[0].point[0] - spots[1].point[0], spots[0].point[1] - spots[1].point[1]) > 30);

    const KeyedSplit made = splitWithKeys(stack, doc, box, level, keys);
    REQUIRE(made.pieces.size() == 2);
    CHECK(made.keys == 2);
    CHECK(made.pins.empty());
    const double tanT = std::tan(2 * pi / 180);
    const double peg = frustum(2.5, 2.5 - 6 * tanT, 6);          // what stands out
    const double hole = frustum(2.7, 2.7 - 6.5 * tanT, 6.5);     // what's cut away
    // Top: half the box less two holes; bottom: half the box and two pegs.
    CHECK_THAT(vol(doc, made.pieces[0]), WithinRel(24000.0 - 2 * hole, 2e-3));
    CHECK_THAT(vol(doc, made.pieces[1]), WithinRel(24000.0 + 2 * peg, 2e-3));
    const Bounds bottom = doc.worldBounds(made.pieces[1]);
    CHECK_THAT(bottom.max[2], WithinAbs(16.0, 1e-3));  // the pegs stand 6 mm above the cut
    // A peg's tip is narrower than its foot: the hole's mouth is wider than the tip by more
    // than the clearance.
    CHECK(2.5 - 6 * tanT + 0.2 < 2.7);

    // One undo puts the box back.
    stack.undo();
    CHECK(doc.isTopLevel(box));
    CHECK_THAT(vol(doc, box), WithinRel(48000.0, 1e-6));
}

TEST_CASE("Print split: loose pins, and cuts too small to key", "[printsplit]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{60, 40, 20});
    KeyOptions pins;
    pins.loosePins = true;
    pins.length = 10;
    const KeyedSplit made = splitWithKeys(stack, doc, box, {{0, 0, 10}, {0, 0, 1}}, pins);
    CHECK(made.keys == 2);
    REQUIRE(made.pins.size() == 2);
    // Holes in both pieces; the pins stand beside the box.
    CHECK(vol(doc, made.pieces[0]) < 24000.0);
    CHECK(vol(doc, made.pieces[1]) < 24000.0);
    const Bounds pin = doc.worldBounds(made.pins[0]);
    CHECK(pin.min[0] > 30.0);
    CHECK_THAT(pin.min[2], WithinAbs(0.0, 1e-3));
    CHECK_THAT(pin.max[2], WithinAbs(10.0, 1e-3));
    CHECK_THAT(pin.size()[0], WithinAbs(5.0, 1e-3));  // as wide as a peg in the middle

    // A thin plate's cut has no room for even a small peg: still cut, just not keyed.
    const ObjectId thin = add(stack, doc, BoxSpec{60, 3, 20}, 200, 0, 0);
    const SplitPlane across{{200, 0, 10}, {1, 0, 0}};
    CHECK(keySpots(doc, thin, across, KeyOptions{}).empty());
    const KeyedSplit plain = splitWithKeys(stack, doc, thin, across, KeyOptions{});
    CHECK(plain.keys == 0);
    CHECK(plain.pieces.size() == 2);
    // A narrower cut gets smaller pegs.
    const ObjectId bar = add(stack, doc, BoxSpec{60, 8, 8}, 400, 0, 0);
    const auto small = keySpots(doc, bar, {{400, 0, 4}, {1, 0, 0}}, KeyOptions{});
    REQUIRE(small.size() == 1);
    CHECK(small[0].diameter < 5.0);
}

TEST_CASE("Smart split: cuts a part too big for the printer into pieces that fit", "[printsplit]")
{
    Document doc;
    CommandStack stack(doc);
    const std::array<double, 3> printer{220, 220, 250};
    KeyOptions keys;

    // Fits already: nothing to do.
    const ObjectId small = add(stack, doc, BoxSpec{100, 100, 50}, -300, 0, 0);
    CHECK(planSmartSplit(doc, small, printer, keys).pieces == 1);
    const std::size_t before = doc.bodyCount();
    CHECK(smartSplit(stack, doc, small, printer, keys).pieces.size() == 1);
    CHECK(doc.bodyCount() == before);

    // 300 long: two pieces, cut across X.
    const ObjectId longBox = add(stack, doc, BoxSpec{300, 100, 50});
    const SmartPlan plan = planSmartSplit(doc, longBox, printer, keys);
    CHECK(plan.pieces == 2);
    CHECK(plan.cutsAcross == std::array<int, 3>{1, 0, 0});
    const KeyedSplit made = smartSplit(stack, doc, longBox, printer, keys);
    REQUIRE(made.pieces.size() == 2);
    CHECK(made.keys >= 2);
    for (ObjectId p : made.pieces) {
        const auto s = doc.worldBounds(p).size();
        CHECK(s[0] <= 219.0 + 1e-3);  // pegs and all
        CHECK(s[1] <= 219.0 + 1e-3);
    }
    stack.undo();
    CHECK(doc.isTopLevel(longBox));

    // A dumbbell: the cut goes through the thin bar, not the heavy ends.
    const ObjectId left = add(stack, doc, BoxSpec{100, 40, 40}, -110, 0, 0);
    const ObjectId right = add(stack, doc, BoxSpec{100, 40, 40}, 110, 0, 0);
    const ObjectId middle = add(stack, doc, BoxSpec{140, 10, 10}, 0, 0, 15);
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Dumbbell";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = {left, right, middle};
    merged.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(merged));
    const KeyedSplit bell = smartSplit(stack, doc, merged.id, printer, keys);
    REQUIRE(bell.pieces.size() == 2);
    const Bounds a = doc.worldBounds(bell.pieces[0]), b = doc.worldBounds(bell.pieces[1]);
    const double cut = std::min(a.max[0], b.max[0]) < std::max(a.min[0], b.min[0])
                           ? 0.0
                           : (a.min[0] > b.min[0] ? a.min[0] : b.min[0]);
    CHECK(cut > -60.0);  // in the bar (the ends reach in to ±60)
    CHECK(cut < 60.0);

    // Too tall and too long: cut both ways.
    const ObjectId tower = add(stack, doc, BoxSpec{60, 300, 400}, 0, 400, 0);
    const SmartPlan tall = planSmartSplit(doc, tower, printer, keys);
    CHECK(tall.pieces == 4);
    const KeyedSplit stacked = smartSplit(stack, doc, tower, printer, keys);
    CHECK(stacked.pieces.size() == 4);
    for (ObjectId p : stacked.pieces) {
        const auto s = doc.worldBounds(p).size();
        CHECK(std::max(s[0], s[1]) <= 219.0 + 1e-3);
        CHECK(s[2] <= 249.0 + 1e-3);
    }
}

TEST_CASE("Split to Fit plans its cuts first: shown, moved, checked, then made", "[printsplit]")
{
    Document doc;
    CommandStack stack(doc);
    const std::array<double, 3> printer{220, 220, 250};
    KeyOptions keys;

    // A long bar leaning 20° (rising along X): too long for the printer in X.
    Body bar;
    bar.id = doc.allocateId();
    bar.name = "Bar";
    bar.spec = BoxSpec{300, 20, 20};
    bar.placement.rotationY = -20.0;
    bar.placement.z = 60.0;
    stack.execute(std::make_unique<AddBodyCommand>(bar));
    const Bounds box = doc.worldBounds(bar.id);

    const FitPlan level = planFitCuts(doc, bar.id, printer, keys, false);
    REQUIRE(level.cuts.size() == 1);
    CHECK(level.cuts[0].axis == 0);
    CHECK(level.cuts[0].tilt == 0.0);
    CHECK(fitProblems(doc, bar.id, level, printer, keys).empty());

    // Angled: the cut leans with the bar, straight across it, so it cuts through less.
    const FitPlan angled = planFitCuts(doc, bar.id, printer, keys, true);
    REQUIRE(angled.cuts.size() == 1);
    CHECK(angled.cuts[0].tilt < 0.0);  // leaning with the bar (10° or 20°: nearly the same)
    CHECK(angled.cuts[0].tiltAxis == 1);
    CHECK(sectionArea(doc, bar.id, planeOf(angled.cuts[0], box)) <
          sectionArea(doc, bar.id, planeOf(level.cuts[0], box)) * 0.97);
    CHECK(fitProblems(doc, bar.id, angled, printer, keys).empty());

    // Moved too far: the piece on one side would be too long.
    FitPlan moved = angled;
    moved.cuts[0].position = box.min[0] + 20.0;
    const auto problems = fitProblems(doc, bar.id, moved, printer, keys);
    REQUIRE_FALSE(problems.empty());
    CHECK(problems.front().find("could be") != std::string::npos);
    moved.cuts[0].position = box.max[0] + 50.0;
    CHECK_FALSE(fitProblems(doc, bar.id, moved, printer, keys).empty());  // misses

    // Made: two keyed pieces that fit, as one undo step.
    const std::size_t before = doc.bodyCount();
    const KeyedSplit made = splitAlong(stack, doc, bar.id, angled.cuts, keys);
    REQUIRE(made.pieces.size() == 2);
    CHECK(made.keys >= 1);
    for (ObjectId p : made.pieces) {
        const auto s = doc.worldBounds(p).size();
        CHECK(std::max(s[0], s[1]) <= 219.0 + 1e-3);
        CHECK(s[2] <= 249.0 + 1e-3);
    }
    stack.undo();
    CHECK(doc.bodyCount() == before);
    CHECK(doc.isTopLevel(bar.id));

    // A grid: every cut splits every piece it passes through.
    const ObjectId slab = add(stack, doc, BoxSpec{400, 400, 30}, 0, 600, 0);
    const FitPlan grid = planFitCuts(doc, slab, printer, keys, false);
    CHECK(grid.cuts.size() == 2);
    CHECK(grid.pieces == 4);
    // Pegs on one cut keep clear of the other, so it doesn't slice through them.
    const Bounds slabBox = doc.worldBounds(slab);
    const SplitPlane first = planeOf(grid.cuts[0], slabBox), second = planeOf(grid.cuts[1], slabBox);
    const auto spots = keySpots(doc, slab, first, keys, {second});
    REQUIRE_FALSE(spots.empty());
    for (const KeySpot& k : spots) {
        double d = 0;
        for (int c = 0; c < 3; ++c)
            d += (k.point[std::size_t(c)] - second.point[std::size_t(c)]) * second.normal[std::size_t(c)];
        CHECK(std::abs(d) >= k.diameter / 2 + keys.clearance);
    }
    CHECK(splitAlong(stack, doc, slab, grid.cuts, keys).pieces.size() == 4);
}
