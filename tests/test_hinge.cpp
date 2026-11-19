// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 50: Quick Hinge.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Hinge.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId addBox(CommandStack& stack, Document& doc, double z, double size = 20.0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{size, size, size};
    b.name = doc.uniqueName("Box");
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId addHalf(CommandStack& stack, Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                 const HingeFinish& f)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Hinged");
    r.boolean = hingeSpec(doc, target, where, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

MeshSolid placed(const Document& doc, ObjectId id)
{
    return meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id));
}

MeshSolid solidOf(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

double overlap(const MeshSolid& a, const MeshSolid& b) { return meshkernel::volume(meshkernel::intersected({a, b})); }

// Turned `degrees` round the pin, whose middle is at `centre`, lying along X or Y.
MeshSolid turned(const MeshSolid& s, const Vec3& centre, const Vec3& axis, double degrees)
{
    const bool alongX = std::abs(axis[0]) > 0.99;
    REQUIRE((alongX || std::abs(axis[1]) > 0.99));
    const Transform turn =
        alongX ? Transform::rotationXYZ(degrees, 0, 0) : Transform::rotationXYZ(0, degrees, 0);
    const Transform t = Transform::translation(centre) * turn * Transform::translation({-centre[0], -centre[1], -centre[2]});
    return meshkernel::transformed(s, t);
}

// The spot on top of a 20 mm box standing on the plate.
const emboss::SurfacePoint kTop{{0, 0, 20}, {0, 0, 1}};

}  // namespace

TEST_CASE("Hinge: the settings are checked in plain words", "[hinge]")
{
    HingeFinish f;
    CHECK_NOTHROW(hinge::check(f));
    HingeFinish thin = f;
    thin.width = 0.5;
    CHECK_THROWS_AS(hinge::check(thin), std::invalid_argument);
    HingeFinish fat = f;
    fat.pinSize = 4.5;  // the end is only 5 across
    CHECK_THROWS_AS(hinge::check(fat), std::invalid_argument);
    HingeFinish swivel;
    swivel.kind = HingeKind::Swivel;
    swivel.width = 4.0;
    swivel.barrel = 5.0;
    CHECK_NOTHROW(hinge::check(swivel));
    swivel.lip = 2.0;  // more than a quarter of the peg
    CHECK_THROWS_AS(hinge::check(swivel), std::invalid_argument);
    // The pin holes: the arms hold the pin, the tab turns on it (snug when stiff).
    CHECK_THAT(hinge::armHole(f), WithinAbs(2.1, 1e-12));
    CHECK_THAT(hinge::tabHole(f), WithinAbs(2.05, 1e-12));
    f.stiff = false;
    CHECK_THAT(hinge::tabHole(f), WithinAbs(2.2, 1e-12));
}

TEST_CASE("Hinge: the tab and pin fit the fork, and it bends", "[hinge]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0);
    const double before = meshkernel::volume(doc.meshSolid(lower));
    HingeFinish f;  // tab 3 thick, end 5 across, printed pin 2 across
    const ObjectId fork = addHalf(stack, doc, lower, kTop, f);
    const MeshSolid forked = placed(doc, fork);
    CHECK(meshkernel::volume(forked) < before - 50.0);  // the slot and pin hole are cut

    const auto parts = hinge::pieces(f, kTop.point, kTop.normal, true);
    REQUIRE(parts.size() == 2);  // the tab and the pin
    CHECK(parts[0].name == "Hinge tab 3 \xC3\x97 \xC3\x98" "5");
    const MeshSolid tab = solidOf(parts[0].mesh);
    const MeshSolid pin = solidOf(parts[1].mesh);
    // Straight, the tab, the pin and the fork don't touch: there's room for printing.
    CHECK(overlap(forked, tab) < 1e-3);
    CHECK(overlap(forked, pin) < 1e-3);
    CHECK(overlap(tab, pin) < 1e-3);
    // The tab stands out of the fork, its end down inside.
    const Bounds tb = meshkernel::bounds(tab);
    CHECK_THAT(tb.max[2], WithinAbs(20.0 + 5.0, 1e-3));  // a stem as long as the end is wide
    CHECK(tb.min[2] < 20.0 - 4.0);
    // The pin goes right through: as long as the slot reaches both ways.
    const Bounds pb = meshkernel::bounds(pin);
    CHECK_THAT(std::max(pb.size()[0], pb.size()[1]), WithinAbs(2.0 * hinge::reachOf(f), 1e-3));

    // It bends 90 degrees both ways without hitting the fork.
    const Vec3 axis = hinge::pinAxis(f, kTop.normal);
    const Vec3 centre{0, 0, 20.0 - hinge::pinDepth(f)};
    CHECK(overlap(forked, turned(tab, centre, axis, 90)) < 1e-3);
    CHECK(overlap(forked, turned(tab, centre, axis, -90)) < 1e-3);
    // Open on one side only, it bends that way and is stopped the other.
    HingeFinish one = f;
    one.swing = HingeSwing::Forward;
    stack.undo();
    const ObjectId fork1 = addHalf(stack, doc, lower, kTop, one);
    const MeshSolid forked1 = placed(doc, fork1);
    const double a = overlap(forked1, turned(tab, centre, axis, 90));
    const double b = overlap(forked1, turned(tab, centre, axis, -90));
    CHECK(std::min(a, b) < 1e-3);
    CHECK(std::max(a, b) > 1.0);
}

TEST_CASE("Hinge: another part's end made into the tab", "[hinge]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0);
    const ObjectId upper = addBox(stack, doc, 20);  // sitting right on it
    HingeFinish f;
    f.link = 7;
    const ObjectId fork = addHalf(stack, doc, lower, kTop, f);
    const hinge::Half other = hinge::otherHalf(f, kTop.point, kTop.normal);
    CHECK(other.finish.half == HingeHalf::Tab);
    const ObjectId tab = addHalf(stack, doc, upper, {other.point, other.normal}, other.finish);
    CHECK(hingePartner(doc, fork) == tab);
    CHECK(hingePartner(doc, tab) == fork);

    const MeshSolid a = placed(doc, fork), b = placed(doc, tab);
    // Room between them everywhere (the upper part stands `fit` clear of the fork) ...
    CHECK(overlap(a, b) < 1e-3);
    const Bounds bb = meshkernel::bounds(b);
    // ... and its tab reaches down into the fork.
    CHECK(bb.min[2] < 20.0 - 4.0);
    // Beside the tab, it stands `fit` clear of the fork's surface, so it can turn.
    Mesh slab;
    {
        const float x0 = 6, x1 = 9, y0 = 6, y1 = 9, z0 = 20.0f, z1 = 20.09f;
        slab.positions = {x0, y0, z0, x1, y0, z0, x0, y1, z0, x1, y1, z0, x0, y0, z1, x1, y0, z1, x0, y1, z1, x1, y1, z1};
        slab.indices = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
    }
    CHECK(overlap(b, solidOf(slab)) < 1e-6);

    // Saved and opened again: both halves, still a pair.
    const auto path = std::filesystem::temp_directory_path() / "forge3d-test-hinge.f3d";
    file::save(doc, path, "test");
    Document reread;
    reread.replaceAll(file::load(path));
    const Body* r = reread.find(tab);
    REQUIRE(r);
    REQUIRE(r->boolean->hinge);
    CHECK(*r->boolean->hinge == *doc.find(tab)->boolean->hinge);
    CHECK(hingePartner(reread, fork) == tab);
    CHECK(history::describe(reread, tab).find("Hinge tab") != std::string::npos);
    CHECK_THAT(meshkernel::volume(reread.meshSolid(tab)), WithinAbs(meshkernel::volume(doc.meshSolid(tab)), 1e-3));
    std::filesystem::remove(path);
}

TEST_CASE("Hinge: a double hinge's link joins two forks", "[hinge]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0);
    const ObjectId upper = addBox(stack, doc, 21);  // 1 mm up: the gap between them
    HingeFinish f;
    f.kind = HingeKind::Double;
    f.gap = 1.0;
    f.turn = 30;
    const ObjectId fork = addHalf(stack, doc, lower, kTop, f);
    const hinge::Half other = hinge::otherHalf(f, kTop.point, kTop.normal);
    CHECK_THAT(other.point[2], WithinAbs(21.0, 1e-9));
    CHECK_THAT(other.normal[2], WithinAbs(-1.0, 1e-9));
    // Its pin lies the same way.
    const Vec3 a1 = hinge::pinAxis(f, kTop.normal), a2 = hinge::pinAxis(other.finish, other.normal);
    CHECK_THAT(std::abs(a1[0] * a2[0] + a1[1] * a2[1] + a1[2] * a2[2]), WithinAbs(1.0, 1e-9));
    const ObjectId fork2 = addHalf(stack, doc, upper, {other.point, other.normal}, other.finish);

    const auto parts = hinge::pieces(f, kTop.point, kTop.normal, true);
    REQUIRE(parts.size() == 3);  // the link and two pins
    const MeshSolid link = solidOf(parts[0].mesh);
    CHECK(overlap(placed(doc, fork), link) < 1e-3);
    CHECK(overlap(placed(doc, fork2), link) < 1e-3);
    CHECK(overlap(link, solidOf(parts[1].mesh)) < 1e-3);
    CHECK(overlap(placed(doc, fork2), solidOf(parts[2].mesh)) < 1e-3);
    // A double hinge's parts each get a fork, never a tab.
    HingeFinish wrong = f;
    wrong.half = HingeHalf::Tab;
    CHECK_THROWS(addHalf(stack, doc, upper, {other.point, other.normal}, wrong));
}

TEST_CASE("Hinge: a swivel's peg snaps into its bore and turns", "[hinge]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0);
    HingeFinish f;
    f.kind = HingeKind::Swivel;
    f.width = 4.0;   // peg across
    f.barrel = 5.0;  // how far in
    f.lip = 0.4;
    const ObjectId bore = addHalf(stack, doc, lower, kTop, f);
    const auto parts = hinge::pieces(f, kTop.point, kTop.normal, true);
    REQUIRE(parts.size() == 1);
    CHECK(parts[0].name == "Swivel peg \xC3\x98" "4");
    const MeshSolid peg = solidOf(parts[0].mesh);
    // Seated, the peg is clear of the bore all round (the lip in its groove).
    CHECK(overlap(placed(doc, bore), peg) < 1e-3);
    // Pulled out a little, the lip catches: it stays in.
    const MeshSolid out = meshkernel::transformed(peg, Transform::translation({0, 0, 1.2}));
    CHECK(overlap(placed(doc, bore), out) > 0.05);
    CHECK(history::describe(doc, bore) == "Swivel bore \xC3\x98" "4 \xC3\x97 5");
}

TEST_CASE("Hinge: scaling evenly grows a printed pin, not a filament one", "[hinge]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0);
    HingeFinish f;
    f.pin = PinKind::Filament;
    f.pinSize = 1.75;
    f.barrel = 4.0;
    const ObjectId fork = addHalf(stack, doc, lower, kTop, f);
    const auto tree = scaledTree(doc, fork, {2.0, 2.0, 2.0}, {0.0, 0.0, 0.0});
    REQUIRE(tree.back().boolean->hinge);
    const HingeFinish& g = *tree.back().boolean->hinge;
    CHECK_THAT(g.width, WithinAbs(6.0, 1e-9));
    CHECK_THAT(g.barrel, WithinAbs(8.0, 1e-9));
    CHECK_THAT(g.pinSize, WithinAbs(1.75, 1e-9));
    CHECK_THAT(g.fit, WithinAbs(0.1, 1e-9));
}
