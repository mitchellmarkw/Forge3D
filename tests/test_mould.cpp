// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 69: Mould Maker.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Mould.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name, double x = 0.0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    b.placement.x = x;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id) { return meshkernel::volume(doc.meshSolid(id)); }

}  // namespace

TEST_CASE("Mould: the outline's area at a height, and where a part is widest", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 10, 6}, "Box");
    const Mesh part = mouldPart(doc, box);
    const Bounds b = part.bounds();
    CHECK_THAT(mould::sectionArea(part, (b.min[2] + b.max[2]) / 2.0), WithinRel(200.0, 1e-6));
    CHECK(mould::sectionArea(part, b.max[2] + 1.0) == 0.0);

    const ObjectId ball = add(stack, doc, SphereSpec{10, 10, 10}, "Ball", 40.0);
    const Mesh round = mouldPart(doc, ball);
    const Bounds rb = round.bounds();
    const double middle = (rb.min[2] + rb.max[2]) / 2.0;
    CHECK_THAT(mould::widest(round), WithinAbs(middle, 0.8));
    CHECK_THAT(mould::sectionArea(round, middle), WithinRel(std::numbers::pi * 100.0, 0.02));
    const Vec3 top = mould::highest(round);
    CHECK_THAT(top[2], WithinAbs(rb.max[2], 0.05));
    CHECK_THAT(top[0], WithinAbs(40.0, 0.5));
}

TEST_CASE("Mould: two halves round a ball, keyed, with a spout", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10, 10, 10}, "Ball");
    const Mesh part = mouldPart(doc, ball);
    const Bounds pb = part.bounds();
    MouldFinish f = mould::suggested(part);
    CHECK(f.vents.empty());  // a ball's only high spot is where the spout goes
    CHECK_THAT(f.spout[2], WithinAbs(pb.max[2], 0.05));

    const std::size_t before = doc.bodies().size();
    const auto halves = makeMould(stack, doc, ball, f);
    REQUIRE(halves.size() == 2);
    CHECK(doc.find(halves[0])->name == "Ball mould (top)");
    CHECK(doc.find(halves[1])->name == "Ball mould (bottom)");
    CHECK(mouldOf(doc, halves[0]).top);
    CHECK_FALSE(mouldOf(doc, halves[1]).top);

    // Together, the box less the ball (less the spout, a little for the keys' fit).
    const double boxVolume = (pb.size()[0] + 16.0) * (pb.size()[1] + 16.0) * (pb.size()[2] + 16.0);
    const double ballVolume = meshkernel::volume(doc.meshSolid(ball));
    const double top = volumeOf(doc, halves[0]), bottom = volumeOf(doc, halves[1]);
    CHECK_THAT(top + bottom, WithinRel(boxVolume - ballVolume, 0.03));
    CHECK(top + bottom < boxVolume - ballVolume);

    // Parted at the ball's middle: the top half starts there; the bottom half's keys
    // stand up out of it. Both lifted so the bottom half stands on the plate.
    const double lift = 8.0 - pb.min[2];
    const Bounds tb = doc.worldBounds(halves[0]), bb = doc.worldBounds(halves[1]);
    CHECK_THAT(tb.min[2], WithinAbs(f.parting + lift, 0.01));
    CHECK_THAT(bb.max[2], WithinAbs(f.parting + mould::keyRadius(f) + lift, 0.01));
    CHECK_THAT(tb.max[2], WithinAbs(pb.max[2] + 8.0 + lift, 0.01));
    CHECK_THAT(bb.min[2], WithinAbs(0.0, 0.01));
    // Changing it: the part where it was, in the mould's own coordinates.
    CHECK_THAT(mouldPart(doc, halves[0]).bounds().min[2], WithinAbs(pb.min[2], 0.01));
    CHECK(history::describe(doc, halves[0]) == "top piece of 2, 8 mm walls, keyed");

    // One undo step takes it all away.
    stack.undo();
    CHECK(doc.bodies().size() == before);
    stack.redo();
    CHECK(doc.find(halves[0]));
}

TEST_CASE("Mould: without keys the halves meet flat; settings checked", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Block");
    MouldFinish f = mould::suggested(mouldPart(doc, box));
    f.keys = false;
    const auto halves = makeMould(stack, doc, box, f);
    CHECK_THAT(doc.worldBounds(halves[1]).max[2] - doc.worldBounds(halves[1]).min[2],
               WithinAbs(f.parting + 8.0, 0.01));
    // A flat top: the spout in the middle of it.
    CHECK_THAT(f.spout[0], WithinAbs(0.0, 0.01));
    CHECK_THAT(f.spout[1], WithinAbs(0.0, 0.01));

    MouldFinish bad = f;
    bad.parting = 100.0;
    CHECK_THROWS_AS(makeMould(stack, doc, box, bad), std::invalid_argument);
    bad = f;
    bad.spout[2] = f.parting - 1.0;
    CHECK_THROWS_AS(makeMould(stack, doc, box, bad), std::invalid_argument);
    bad = f;
    bad.wall = 1.0;
    CHECK_THROWS_AS(mould::check(bad), std::invalid_argument);
}

TEST_CASE("Mould: air vents at the other high spots", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId big = add(stack, doc, SphereSpec{8, 8, 8}, "Big", -15.0);
    const ObjectId small = add(stack, doc, SphereSpec{5, 5, 5}, "Small", 12.0);
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Pair";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = {big, small};
    merged.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(merged));
    const Mesh part = mouldPart(doc, merged.id);
    const MouldFinish f = mould::suggested(part);
    CHECK_THAT(f.spout[0], WithinAbs(-15.0, 1.0));
    REQUIRE(f.vents.size() == 1);
    CHECK_THAT(f.vents[0][0], WithinAbs(12.0, 1.0));

    const auto halves = makeMould(stack, doc, merged.id, f);
    const double withVents = volumeOf(doc, halves[0]);
    MouldFinish none = mouldOf(doc, halves[0]);
    none.vents.clear();
    const auto changed = withMould(doc, halves[0], none);
    REQUIRE(changed.size() == 2);  // both halves
    CHECK(changed[0].boolean->mould->top);
    CHECK_FALSE(changed[1].boolean->mould->top);
    stack.execute(std::make_unique<EditBodiesCommand>(std::vector<Body>{*doc.find(halves[0]), *doc.find(halves[1])},
                                                       changed, "Change mould"));
    CHECK(volumeOf(doc, halves[0]) > withVents);

    // Thinner walls: the bottom half still stands on the plate.
    MouldFinish thin = mouldOf(doc, halves[1]);
    thin.wall = 5.0;
    stack.execute(std::make_unique<EditBodiesCommand>(std::vector<Body>{*doc.find(halves[1]), *doc.find(halves[0])},
                                                       withMould(doc, halves[1], thin), "Change mould"));
    CHECK_THAT(doc.worldBounds(halves[1]).min[2], WithinAbs(0.0, 0.01));
    CHECK(mouldOf(doc, halves[0]).wall == 5.0);
}

TEST_CASE("Mould: kept in the file", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10, 10, 10}, "Ball");
    MouldFinish f = mould::suggested(mouldPart(doc, ball));
    f.wall = 6.0;
    f.keyFit = 0.3;
    f.vents.push_back({3.0, 0.0, 15.0});
    const auto halves = makeMould(stack, doc, ball, f);
    const std::string json = file::toJson(doc, "test");
    Document back;
    back.replaceAll(file::fromJson(json));
    const Body* top = back.find(halves[0]);
    REQUIRE(top);
    REQUIRE(top->boolean->mould);
    CHECK(*top->boolean->mould == mouldOf(doc, halves[0]));
    CHECK_THAT(meshkernel::volume(back.meshSolid(halves[1])), WithinRel(volumeOf(doc, halves[1]), 1e-6));
}

TEST_CASE("Mould: an upright split, left and right", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10, 10, 10}, "Ball");
    const Mesh part = mouldPart(doc, ball);
    const Bounds pb = part.bounds();
    MouldFinish f = mould::suggested(part);
    f.axis = 0;
    f.parting = mould::widest(part, 0);
    CHECK_THAT(f.parting, WithinAbs(0.0, 0.8));
    const auto halves = makeMould(stack, doc, ball, f);
    REQUIRE(halves.size() == 2);
    CHECK(doc.find(halves[0])->name == "Ball mould (right)");
    CHECK(doc.find(halves[1])->name == "Ball mould (left)");
    const Bounds right = doc.worldBounds(halves[0]), left = doc.worldBounds(halves[1]);
    CHECK_THAT(right.min[0], WithinAbs(f.parting, 0.01));
    CHECK_THAT(left.max[0], WithinAbs(f.parting + mould::keyRadius(f), 0.01));  // its pegs
    CHECK_THAT(left.min[2], WithinAbs(0.0, 0.01));
    CHECK_THAT(right.min[2], WithinAbs(0.0, 0.01));
    const double boxVolume = (pb.size()[0] + 16.0) * (pb.size()[1] + 16.0) * (pb.size()[2] + 16.0);
    const double ballVolume = meshkernel::volume(doc.meshSolid(ball));
    CHECK_THAT(volumeOf(doc, halves[0]) + volumeOf(doc, halves[1]), WithinRel(boxVolume - ballVolume, 0.03));
    // Split through the middle, a ball has no undercuts.
    const mould::Undercuts u = mould::undercuts(part, f);
    CHECK(u.area < 0.01 * u.total);
}

TEST_CASE("Mould: undercuts, and the split with the fewest", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    // A mushroom: a narrow stem under a wide cap.
    const ObjectId stem = add(stack, doc, BoxSpec{20, 20, 10}, "Stem");
    Body cap;
    cap.id = doc.allocateId();
    cap.spec = BoxSpec{40, 40, 5};
    cap.name = "Cap";
    cap.placement.z = 10.0;
    stack.execute(std::make_unique<AddBodyCommand>(cap));
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Mushroom";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = {stem, cap.id};
    merged.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(merged));
    const Mesh part = mouldPart(doc, merged.id);

    // Parted through the stem: the top half catches under the cap (and round the stem
    // below it).
    MouldFinish low = mould::suggested(part);
    low.axis = 2;
    low.parting = 5.0;
    const mould::Undercuts bad = mould::undercuts(part, low);
    CHECK_THAT(bad.area, WithinRel(1200.0 + 400.0, 0.02));
    CHECK_FALSE(mould::undercutMesh(part, bad).empty());
    // Parted through the cap: none.
    MouldFinish high = low;
    high.parting = 12.5;
    CHECK(mould::undercuts(part, high).area < 1.0);

    // The best split: level, through the cap (the widest outline with no undercut).
    const MouldFinish best = mould::bestSplit(part, low);
    CHECK(best.axis == 2);
    CHECK(best.parting > 10.0);
    CHECK(best.parting < 15.0);
    CHECK(mould::undercuts(part, best).area < 1.0);
}

TEST_CASE("Mould: three and four pieces", "[mould]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10, 10, 10}, "Ball");
    const Mesh part = mouldPart(doc, ball);
    const Bounds pb = part.bounds();
    const double boxVolume = (pb.size()[0] + 16.0) * (pb.size()[1] + 16.0) * (pb.size()[2] + 16.0);
    const double ballVolume = meshkernel::volume(doc.meshSolid(ball));
    MouldFinish f = mould::suggested(part);
    f.second = true;
    f.axis2 = 0;
    f.parting2 = 0.0;
    f.secondIn = 0;
    CHECK(mould::pieces(f).size() == 4);
    f.secondIn = 1;
    CHECK(mould::pieces(f).size() == 3);
    f.axis2 = 2;
    CHECK_THROWS_AS(mould::check(mould::pieces(f).front()), std::invalid_argument);
    f.axis2 = 0;

    SECTION("three: the top half cut again")
    {
        const auto made = makeMould(stack, doc, ball, f);
        REQUIRE(made.size() == 3);
        CHECK(doc.find(made[0])->name == "Ball mould (top, right)");
        CHECK(doc.find(made[1])->name == "Ball mould (top, left)");
        CHECK(doc.find(made[2])->name == "Ball mould (bottom)");
        double total = 0.0;
        for (ObjectId id : made)
            total += volumeOf(doc, id);
        CHECK_THAT(total, WithinRel(boxVolume - ballVolume, 0.04));
        // The top-left piece has pegs into the top-right one.
        CHECK(doc.worldBounds(made[1]).max[0] > 0.5);
    }
    SECTION("four, changed together")
    {
        f.secondIn = 0;
        const auto made = makeMould(stack, doc, ball, f);
        REQUIRE(made.size() == 4);
        CHECK(doc.find(made[3])->name == "Ball mould (bottom, left)");
        double total = 0.0;
        for (ObjectId id : made)
            total += volumeOf(doc, id);
        CHECK_THAT(total, WithinRel(boxVolume - ballVolume, 0.04));
        MouldFinish thick = mouldOf(doc, made[2]);
        thick.wall = 10.0;
        const auto changed = withMould(doc, made[2], thick);
        CHECK(changed.size() == 4);
        for (const Body& b : changed)
            CHECK(b.boolean->mould->wall == 10.0);
        MouldFinish turned = thick;
        turned.axis2 = 1;
        CHECK_THROWS_AS(withMould(doc, made[2], turned), std::invalid_argument);
    }
}
