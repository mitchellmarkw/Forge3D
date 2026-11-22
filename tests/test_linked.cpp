// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 74: linked mirrors.
#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Port.h"
#include "forge/core/Scale.h"

#include <cstdlib>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

struct Bench {
    Document doc;
    CommandStack stack{doc};
    ObjectId arm = kInvalidId;
    Bench()
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = BoxSpec{10, 4, 6};
        b.name = "Left arm";
        b.placement.x = 20.0;  // 15..25 across
        stack.execute(std::make_unique<AddBodyCommand>(b));
        arm = b.id;
    }
};

}  // namespace

TEST_CASE("Linked mirror: the other side's name", "[linked]")
{
    if (const char* out = std::getenv("FORGE_LINK_DUMP")) {
        Bench bench;
        Body torso;
        torso.id = bench.doc.allocateId();
        torso.spec = BoxSpec{20, 10, 30};
        torso.name = "Torso";
        bench.stack.execute(std::make_unique<AddBodyCommand>(torso));
        file::save(bench.doc, out, "test");
    }
    CHECK(otherSideName("Left arm") == "Right arm");
    CHECK(otherSideName("right leg") == "left leg");
    CHECK(otherSideName("Arm (LEFT)") == "Arm (RIGHT)");
    CHECK(otherSideName("Leftover") == "Leftover (mirrored)");
    CHECK(otherSideName("Wing") == "Wing (mirrored)");
}

TEST_CASE("Linked mirror: keeps up with the shape it mirrors", "[linked]")
{
    Bench bench;
    Document& doc = bench.doc;
    const ObjectId right = addLinkedMirror(bench.stack, doc, bench.arm, 0, 0.0);
    const Body* r = doc.find(right);
    REQUIRE(r);
    CHECK(r->name == "Right arm");
    CHECK(doc.isTopLevel(bench.arm));  // the left arm stays itself
    CHECK(history::describe(doc, right) == "Mirror of 'Left arm', left / right, kept up to date");
    Bounds b = doc.worldBounds(right);
    CHECK_THAT(b.min[0], WithinAbs(-25.0, 1e-4));
    CHECK_THAT(b.max[0], WithinAbs(-15.0, 1e-4));
    CHECK_THAT(meshkernel::volume(doc.meshSolid(right)), WithinRel(240.0, 1e-6));

    // Made longer: the mirror too.
    Body longer = *doc.find(bench.arm);
    longer.spec = BoxSpec{14, 4, 6};
    bench.stack.execute(std::make_unique<EditBodyCommand>(*doc.find(bench.arm), longer, "Resize"));
    b = doc.worldBounds(right);
    CHECK_THAT(b.min[0], WithinAbs(-27.0, 1e-4));

    // Moved further out: the mirror goes the other way.
    Placement out = doc.find(bench.arm)->placement;
    out.x = 30.0;
    bench.stack.execute(std::make_unique<SetPlacementCommand>(bench.arm, out, false));
    CHECK_THAT(doc.worldBounds(right).min[0], WithinAbs(-37.0, 1e-4));

    // A step added on the left arm (a peg on its end) shows on the right.
    PortFinish peg;
    peg.size = 2.0;
    peg.length = 3.0;
    Body pegged;
    pegged.id = doc.allocateId();
    pegged.name = "Left arm";
    pegged.boolean = portSpec(doc, bench.arm, {{37.0, 0.0, 3.0}, {1.0, 0.0, 0.0}}, peg);
    bench.stack.execute(std::make_unique<CombineCommand>(pegged));
    CHECK_THAT(doc.worldBounds(right).min[0], WithinAbs(-40.0, 1e-3));
    bench.stack.undo();
    CHECK_THAT(doc.worldBounds(right).min[0], WithinAbs(-37.0, 1e-4));

    // The mirror itself can be moved on from where it is.
    Placement shifted = doc.find(right)->placement;
    shifted.y = 10.0;
    bench.stack.execute(std::make_unique<SetPlacementCommand>(right, shifted, false));
    CHECK_THAT(doc.worldBounds(right).min[1], WithinAbs(8.0, 1e-4));

    // Its mirror moved: front / back at Y = 5.
    bench.stack.execute(std::make_unique<EditBodyCommand>(*doc.find(right), withLinkPlane(doc, right, 1, 5.0),
                                                          "Move mirror"));
    b = doc.worldBounds(right);
    CHECK_THAT(b.min[0], WithinAbs(23.0, 1e-4));   // on the same side as the arm now
    CHECK_THAT(b.min[1], WithinAbs(18.0, 1e-4));   // 10 - 2 mirrored to 10 + 8, then moved 10

    // Resizing it is refused (it follows the other).
    CHECK_THROWS_AS(scaledTree(doc, right, {2, 2, 2}, {0, 0, 0}), std::invalid_argument);
}

TEST_CASE("Linked mirror: the other shape gone, saved, unlinked", "[linked]")
{
    Bench bench;
    Document& doc = bench.doc;
    const ObjectId right = addLinkedMirror(bench.stack, doc, bench.arm, 0, 0.0);

    SECTION("kept in the file")
    {
        Document back;
        back.replaceAll(file::fromJson(file::toJson(doc, "test")));
        REQUIRE(back.find(right));
        CHECK(*back.find(right) == *doc.find(right));
        CHECK_THAT(back.worldBounds(right).min[0], WithinAbs(-25.0, 1e-4));
    }
    SECTION("its shape deleted: empty, with a problem; undo brings it back")
    {
        bench.stack.execute(std::make_unique<RemoveBodyCommand>(bench.arm));
        CHECK(meshkernel::isEmpty(doc.meshSolid(right)));
        REQUIRE(doc.problemWith(right));
        bench.stack.undo();
        CHECK_FALSE(meshkernel::isEmpty(doc.meshSolid(right)));
        CHECK_FALSE(doc.problemWith(right));
    }
    SECTION("unlinked: a shape of its own, as it was")
    {
        const ObjectId own = unlinkMirror(bench.stack, doc, right);
        CHECK(doc.find(right) == nullptr);
        CHECK(doc.find(own)->name == "Right arm");
        CHECK_THAT(doc.worldBounds(own).min[0], WithinAbs(-25.0, 1e-3));
        Body longer = *doc.find(bench.arm);
        longer.spec = BoxSpec{14, 4, 6};
        bench.stack.execute(std::make_unique<EditBodyCommand>(*doc.find(bench.arm), longer, "Resize"));
        CHECK_THAT(doc.worldBounds(own).min[0], WithinAbs(-25.0, 1e-3));  // no longer follows
        bench.stack.undo();
        bench.stack.undo();
        CHECK(doc.find(right));
    }
}
