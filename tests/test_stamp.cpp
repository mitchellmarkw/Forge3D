// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 59: surface stamps.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Stamp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name, double x = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    b.placement.x = x;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

StampSpot onTop(double x, double y, double z)
{
    StampSpot s;
    s.point = {x, y, z};
    s.normal = {0, 0, 1};
    s.along = {1, 0, 0};
    return s;
}

ObjectId stampOn(CommandStack& stack, Document& doc, ObjectId target, const StampFinish& f, ObjectId custom = kInvalidId)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Stamped");
    r.boolean = stampSpec(doc, target, f, custom);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

}  // namespace

TEST_CASE("Stamps: every kind is a solid, the right size", "[stamp]")
{
    for (int k = 0; k < stamp::kKindCount - 1; ++k) {  // all but Custom
        StampFinish f;
        f.kind = StampKind(k);
        f.size = 4.0;
        f.height = stamp::usualHeight(f.kind, f.size);
        INFO(stamp::title(f.kind));
        const MeshSolid up = stamp::shape(f);
        CHECK(meshkernel::volume(up) > 0.0);
        Bounds b = meshkernel::bounds(up);
        CHECK(b.max[0] - b.min[0] <= 4.0 + 1e-4);
        CHECK(b.max[1] - b.min[1] <= 4.0 + 1e-4);
        const double top = f.kind == StampKind::Screw ? 0.05 : 1e-3;  // the slot takes the very top
        CHECK_THAT(b.max[2], WithinAbs(f.height, top));
        CHECK(b.min[2] < -1.0);  // the stem, into the surface
        f.sunk = true;
        b = meshkernel::bounds(stamp::shape(f));
        CHECK_THAT(b.min[2], WithinAbs(-f.height, top));
        CHECK(b.max[2] > 1.0);  // the stem, out of it
        CHECK(stamp::fromKey(stamp::key(f.kind)) == f.kind);
    }
}

TEST_CASE("Stamps: rivets raised on a box, and sunk", "[stamp]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Plate");
    const double before = meshkernel::volume(doc.meshSolid(box));
    StampFinish f;
    f.size = 3.0;
    f.height = 1.0;
    f.spots = {onTop(-5, 0, 10), onTop(0, 0, 10), onTop(5, 0, 10)};
    const ObjectId raised = stampOn(stack, doc, box, f);
    const double dome = 2.0 / 3.0 * std::numbers::pi * 1.5 * 1.5 * 1.0;
    CHECK_THAT(meshkernel::volume(doc.meshSolid(raised)) - before, WithinRel(3 * dome, 0.12));
    CHECK(history::describe(doc, raised) == "3 rivets \xC3\x98" "3, raised");
    CHECK_THAT(doc.worldBounds(raised).max[2], WithinAbs(11.0, 1e-3));

    // Sunk instead: the same amount cut away.
    StampFinish sunk = stampInPlate(doc, raised);
    sunk.sunk = true;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(raised), withStamp(doc, raised, sunk), "Change"));
    CHECK_THAT(before - meshkernel::volume(doc.meshSolid(raised)), WithinRel(3 * dome, 0.12));
    CHECK_THAT(doc.worldBounds(raised).max[2], WithinAbs(10.0, 1e-3));
}

TEST_CASE("Stamps: rows, turning, a custom shape", "[stamp]")
{
    const auto row = stamp::rowPoints({0, 0, 0}, {20, 0, 0}, 5.0);
    REQUIRE(row.size() == 5);
    CHECK_THAT(row.back()[0], WithinAbs(20.0, 1e-9));
    CHECK(stamp::rowPoints({0, 0, 0}, {1, 0, 0}, 5.0).size() == 1);
    CHECK(stamp::rowPoints({0, 0, 0}, {18, 0, 0}, 5.0).size() == 5);  // spread to land on the end

    // A stitch lies along its row; turned 90° it lies across.
    StampFinish f;
    f.kind = StampKind::Stitch;
    f.size = 4.0;
    f.height = 0.5;
    StampSpot s = onTop(0, 0, 0);
    s.along = {0, 1, 0};
    f.spots = {s};
    Bounds b = meshkernel::bounds(stamp::placed(f));
    CHECK(b.max[1] - b.min[1] > 3.9);
    CHECK(b.max[0] - b.min[0] < 1.0);
    f.spin = 90;
    b = meshkernel::bounds(stamp::placed(f));
    CHECK(b.max[0] - b.min[0] > 3.9);

    // A custom stamp: a 10 mm cube fitted to 4 across and 2 tall, a little sunk in.
    Document doc;
    CommandStack stack(doc);
    const ObjectId plate = add(stack, doc, BoxSpec{30, 30, 5}, "Plate");
    const ObjectId cube = add(stack, doc, BoxSpec{10, 10, 10}, "Badge", 40);
    const double before = meshkernel::volume(doc.meshSolid(plate));
    StampFinish c;
    c.kind = StampKind::Custom;
    c.size = 4.0;
    c.height = 2.0;
    c.spots = {onTop(0, 0, 5)};
    CHECK_THROWS_AS(stampSpec(doc, plate, c), std::invalid_argument);  // needs its shape
    const ObjectId stamped = stampOn(stack, doc, plate, c, cube);
    CHECK_THAT(meshkernel::volume(doc.meshSolid(stamped)) - before, WithinRel(4.0 * 4.0 * 1.7, 0.02));
    CHECK(history::describe(doc, stamped) == "Badge \xC3\x98" "4, raised");
    CHECK_FALSE(doc.isTopLevel(cube));  // taken into the step
}

TEST_CASE("Stamps: saved, and checked", "[stamp]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Plate");
    StampFinish f;
    f.kind = StampKind::Vent;
    f.size = 6.0;
    f.height = 0.8;
    f.sunk = true;
    f.spin = 15;
    f.spacing = 8;
    f.spots = {onTop(0, 0, 10)};
    const ObjectId stamped = stampOn(stack, doc, box, f);
    const auto path = std::filesystem::temp_directory_path() / "forge_stamp_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    const auto found = std::find_if(back.begin(), back.end(), [&](const Body& b) { return b.id == stamped; });
    REQUIRE(found != back.end());
    REQUIRE(found->boolean->stamp);
    CHECK(*found->boolean->stamp == *doc.find(stamped)->boolean->stamp);

    f.size = 0.0;
    CHECK_THROWS_AS(stamp::check(f), std::invalid_argument);
    f.size = 3.0;
    f.height = 0.0;
    CHECK_THROWS_AS(stamp::check(f), std::invalid_argument);
}
