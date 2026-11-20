// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 58: Hollow with drain holes.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/Hollow.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId hollowOut(CommandStack& stack, Document& doc, ObjectId target, const HollowFinish& f)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Hollowed");
    r.boolean = hollowSpec(doc, target, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

}  // namespace

TEST_CASE("Hollow: a box's walls, and a drain hole", "[hollow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, "Box");
    HollowFinish f;
    f.thickness = 2.0;
    const ObjectId hollow = hollowOut(stack, doc, box, f);
    const double walls = meshkernel::volume(doc.meshSolid(hollow));
    CHECK_THAT(walls, WithinRel(8000.0 - 16.0 * 16.0 * 16.0, 0.06));
    CHECK(history::describe(doc, hollow) == "2 mm walls, no drain holes");

    // A hole in the middle of the bottom: a little more comes out (through the wall).
    HollowFinish withHole = hollowInPlate(doc, hollow);
    withHole.holes.push_back({{0, 0, 0}, {0, 0, -1}});
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(hollow), withHollow(doc, hollow, withHole), "Change"));
    const double holed = meshkernel::volume(doc.meshSolid(hollow));
    const double hole = std::numbers::pi * 1.5 * 1.5 * 2.0;
    CHECK_THAT(walls - holed, WithinRel(hole, 0.3));
    CHECK(history::describe(doc, hollow) == "2 mm walls, 1 drain hole \xC3\x98" "3");
}

TEST_CASE("Hollow: a sphere, and a part too thin to hollow", "[hollow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10}, "Ball");
    HollowFinish f;
    f.thickness = 2.0;
    const ObjectId hollow = hollowOut(stack, doc, ball, f);
    const double expected = 4.0 / 3.0 * std::numbers::pi * (1000.0 - 512.0);
    CHECK_THAT(meshkernel::volume(doc.meshSolid(hollow)), WithinRel(expected, 0.08));

    // A slab 3 mm thick has no room inside for 2 mm walls: it stays solid.
    const ObjectId slab = add(stack, doc, BoxSpec{30, 30, 3}, "Slab");
    const ObjectId flat = hollowOut(stack, doc, slab, f);
    CHECK_THAT(meshkernel::volume(doc.meshSolid(flat)), WithinRel(2700.0, 0.01));
}

TEST_CASE("Hollow: holes placed low on their own, plugs, checks", "[hollow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 20, 20}, "Box");
    const Mesh shown = *doc.mesh(box);
    const auto spots = hollow::lowSpots(shown, 3.0, 2);
    REQUIRE(spots.size() == 2);
    for (const DrainHole& s : spots) {
        CHECK_THAT(s.point[2], WithinAbs(0.0, 1e-6));  // on the bottom
        CHECK(s.normal[2] < -0.5);                      // facing down / out
    }
    const double dx = spots[0].point[0] - spots[1].point[0], dy = spots[0].point[1] - spots[1].point[1];
    CHECK(dx * dx + dy * dy >= 12.0 * 12.0);

    HollowFinish f;
    f.holeSize = 3.0;
    f.thickness = 2.0;
    const auto p = std::get<CylinderSpec>(hollow::plug(f, 0.1));
    CHECK_THAT(p.radiusX, WithinAbs(1.4, 1e-9));
    CHECK_THAT(p.height, WithinAbs(2.0, 1e-9));

    f.thickness = 0.1;
    CHECK_THROWS_AS(hollow::check(f), std::invalid_argument);
    f.thickness = 2.0;
    f.holeSize = 50;
    CHECK_THROWS_AS(hollow::check(f), std::invalid_argument);
}

TEST_CASE("Hollow: saved, and rescaled", "[hollow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, "Box");
    HollowFinish f;
    f.thickness = 1.5;
    f.holeSize = 2.5;
    f.holes.push_back({{0, 0, 0}, {0, 0, -1}});
    f.holes.push_back({{10, 0, 10}, {1, 0, 0}});
    const ObjectId hollow = hollowOut(stack, doc, box, f);

    const auto path = std::filesystem::temp_directory_path() / "forge_hollow_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    const auto found = std::find_if(back.begin(), back.end(), [&](const Body& b) { return b.id == hollow; });
    REQUIRE(found != back.end());
    REQUIRE(found->boolean->hollow);
    CHECK(*found->boolean->hollow == *doc.find(hollow)->boolean->hollow);

    ScaleOptions keep;
    keep.keepWalls = true;
    keep.keepJoints = true;
    auto after = rescaledDesign(doc, {hollow}, 0.5, {0, 0, 0}, keep);
    std::vector<Body> before;
    for (const Body& b : after)
        before.push_back(*doc.find(b.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Rescale"));
    const HollowFinish& h = *doc.find(hollow)->boolean->hollow;
    CHECK_THAT(h.thickness, WithinAbs(1.5, 1e-9));
    CHECK_THAT(h.holeSize, WithinAbs(2.5, 1e-9));
    CHECK_THAT(h.holes[1].point[0], WithinAbs(5.0, 1e-9));  // the side moved in
}

TEST_CASE("Hollow: a figure-sized part (timing)", "[.bench][hollow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{75}, "Ball");
    HollowFinish f;
    f.thickness = 2.0;
    const auto start = std::chrono::steady_clock::now();
    const ObjectId hollow = hollowOut(stack, doc, ball, f);
    const auto mesh = doc.mesh(hollow);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    WARN("150 mm ball, 2 mm walls: " << s << " s, " << mesh->triangleCount() << " triangles");
}
