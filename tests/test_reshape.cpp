// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 102: the Reshape box.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Reshape.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Reshape: the lattice", "[reshape]")
{
    Bounds b;
    b.expand(0, 0, 0);
    b.expand(10, 20, 30);
    ReshapeFinish f = reshape::around(b, 3);
    REQUIRE(f.moves.size() == 27);
    CHECK_NOTHROW(reshape::check(f));
    // Nothing moved: nothing moves.
    const Vec3 p{3, 7, 11};
    const Vec3 q = reshape::moved(f, p);
    for (int k = 0; k < 3; ++k)
        CHECK_THAT(q[std::size_t(k)], WithinAbs(p[std::size_t(k)], 1e-12));
    // All handles moved the same way: everything moves that way.
    for (auto& m : f.moves)
        m = {1, 2, 3};
    const Vec3 r = reshape::moved(f, p);
    CHECK_THAT(r[0], WithinAbs(4, 1e-9));
    CHECK_THAT(r[2], WithinAbs(14, 1e-9));
    // One corner handle pulls its corner most, the far corner not at all.
    f = reshape::around(b, 2);
    const int corner = reshape::handleIndex(f, 1, 1, 1);
    f.moves[std::size_t(corner)] = {5, 0, 0};
    CHECK_THAT(reshape::moved(f, reshape::rest(f, corner))[0], WithinAbs(reshape::rest(f, corner)[0] + 5, 1e-9));
    CHECK_THAT(reshape::moved(f, reshape::rest(f, 0))[0], WithinAbs(reshape::rest(f, 0)[0], 1e-9));
    // Mirrored across X.
    CHECK(reshape::mirrored(f, reshape::handleIndex(f, 0, 1, 0)) == reshape::handleIndex(f, 1, 1, 0));
    f.count[0] = 5;
    CHECK_THROWS_AS(reshape::check(f), std::invalid_argument);
}

TEST_CASE("Reshape: a step that bends a box, saved and rescaled", "[reshape]")
{
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Box";
    box.spec = BoxSpec{20, 20, 20};
    stack.execute(std::make_unique<AddBodyCommand>(box));
    ReshapeFinish f = reshape::around(doc.worldBounds(box.id), 3);
    // The top middle handle pulled up 6 mm: the top bulges up in the middle.
    f.moves[std::size_t(reshape::handleIndex(f, 1, 1, 2))] = {0, 0, 6};
    Body step;
    step.id = doc.allocateId();
    step.name = "Reshaped";
    step.boolean = reshapeSpec(doc, box.id, f);
    stack.execute(std::make_unique<CombineCommand>(step));
    const Bounds w = doc.worldBounds(step.id);
    // Bernstein weight of the middle handle at the top middle: 1/2 x 1/2 x 1 = 1/4 of 6.
    CHECK_THAT(double(w.max[2]), WithinAbs(20.0 + 1.5, 0.3));
    CHECK_THAT(double(w.size()[0]), WithinAbs(20.0, 0.05));
    CHECK(meshkernel::volume(doc.meshSolid(step.id)) > 8000.0 + 20.0);
    CHECK(history::describe(doc, step.id) == "3 × 3 × 3 handles, 1 moved");

    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    CHECK(*other.find(step.id) == *doc.find(step.id));
    CHECK_THAT(meshkernel::volume(other.meshSolid(step.id)), WithinRel(meshkernel::volume(doc.meshSolid(step.id)), 1e-9));

    // Twice the size: the bulge twice as tall.
    const auto scaled = scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0});
    for (const Body& b : scaled)
        if (b.id == step.id)
            CHECK_THAT(b.boolean->reshape->moves[std::size_t(reshape::handleIndex(f, 1, 1, 2))][2], WithinAbs(12.0, 1e-9));
    stack.undo();
    CHECK(doc.isTopLevel(box.id));
}
