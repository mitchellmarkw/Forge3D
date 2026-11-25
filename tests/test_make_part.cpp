// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 95: Make Part from paint.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MakePart.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Ball");
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

// How much two shapes overlap on the plate (mm³).
double overlap(const Document& doc, ObjectId a, ObjectId b)
{
    return meshkernel::volume(meshkernel::intersected({meshkernel::transformed(doc.meshSolid(a), doc.worldTransform(a)),
                                                       meshkernel::transformed(doc.meshSolid(b), doc.worldTransform(b))}));
}

// The top of a ball painted: everything within `r` of its top point.
std::vector<MaskDab> capPaint(const Bounds& b, double r)
{
    const Vec3 top{(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, b.max[2]};
    return {{top, r, false}};
}

}  // namespace

TEST_CASE("Make part: a slab over a flat patch", "[makepart]")
{
    // A 10 × 10 square facing up, in four triangles round its middle.
    Mesh square;
    square.positions = {0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0, 5, 5, 0};
    square.indices = {0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4};
    const Mesh s = makepart::slab(square, 0.5, 2.0);
    const MeshSolid solid = meshkernel::fromTriangles(s.positions, s.indices);
    CHECK_THAT(meshkernel::volume(solid), WithinRel(150.0, 1e-6));
    const Bounds b = meshkernel::bounds(solid);
    CHECK_THAT(b.min[2], WithinAbs(0.5, 1e-6));
    CHECK_THAT(b.max[2], WithinAbs(2.0, 1e-6));
    // Its edge brought in 1 mm all round: 8 × 8 (the corners come in along both sides).
    const Mesh in = makepart::slab(square, -1.5, 0.0, 1.0);
    const MeshSolid inSolid = meshkernel::fromTriangles(in.positions, in.indices);
    CHECK(meshkernel::volume(inSolid) < 150.0);
    CHECK(meshkernel::volume(inSolid) > 0.6 * 150.0);
    CHECK_THAT(meshkernel::bounds(inSolid).max[2], WithinAbs(0.0, 1e-6));
    CHECK_THROWS_AS(makepart::slab(Mesh{}, 0, 1), std::invalid_argument);
}

TEST_CASE("Make part: armor sitting on top", "[makepart]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10});
    const double before = volumeOf(doc, ball);
    const Bounds b = doc.worldBounds(ball);
    makepart::Options o;
    o.thickness = 1.5;
    o.fit = 0.2;
    const makepart::Made made = makepart::makePart(stack, doc, ball, capPaint(b, 8.0), o);
    CHECK(stack.undoLabel() == "Make part from Ball");
    REQUIRE(doc.find(made.part));
    CHECK(doc.find(made.part)->name == "Ball part");
    CHECK(made.body == ball);
    CHECK_THAT(volumeOf(doc, ball), WithinRel(before, 1e-9));  // the body as it was
    // Over the top: from the fit gap out by the thickness.
    const Bounds p = doc.worldBounds(made.part);
    CHECK_THAT(p.max[2], WithinAbs(b.max[2] + 1.7, 0.05));
    CHECK(p.min[2] > b.max[2] - 8.0);  // only over the painted cap
    // About the cap's area (2πrh, h the cap's height) times the thickness, a little more
    // for being further out.
    const double h = 8.0 * 8.0 / 20.0;
    const double shell = 2.0 * std::numbers::pi * 10.0 * h * 1.5;
    CHECK(volumeOf(doc, made.part) > 0.9 * shell);
    CHECK(volumeOf(doc, made.part) < 1.5 * shell);
    // Clear of the body all over (by the fit).
    CHECK(overlap(doc, made.part, ball) < 1e-3);
    // Saved and read back.
    const auto path = std::filesystem::temp_directory_path() / "makepart.f3d";
    file::save(doc, path, "test");
    Document reread;
    reread.replaceAll(file::load(path));
    CHECK_THAT(volumeOf(reread, made.part), WithinRel(volumeOf(doc, made.part), 1e-6));
    // One undo.
    stack.undo();
    CHECK(!doc.find(made.part));
}

TEST_CASE("Make part: set into the body", "[makepart]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10});
    const double before = volumeOf(doc, ball);
    const Bounds b = doc.worldBounds(ball);
    makepart::Options o;
    o.setIn = true;
    o.thickness = 1.5;
    o.fit = 0.1;
    const makepart::Made made = makepart::makePart(stack, doc, ball, capPaint(b, 8.0), o);
    REQUIRE(doc.find(made.part));
    REQUIRE(made.body != ball);  // the body cut back: a new step
    CHECK(doc.isTopLevel(made.body));
    // Flush: the part's top where the ball's was; the body lower under it.
    CHECK_THAT(doc.worldBounds(made.part).max[2], WithinAbs(b.max[2], 0.05));
    CHECK(doc.worldBounds(made.body).max[2] < b.max[2] - 1.5);
    // Together about the ball (less the fit gaps).
    const double vp = volumeOf(doc, made.part), vb = volumeOf(doc, made.body);
    CHECK(vb < before - vp);
    CHECK(vb + vp > 0.95 * before);
    CHECK(overlap(doc, made.part, made.body) < 1e-3);
    stack.undo();
    CHECK(doc.isTopLevel(ball));
    CHECK(!doc.find(made.part));
}

TEST_CASE("Make part: refused in plain words", "[makepart]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10});
    makepart::Options o;
    CHECK_THROWS_AS(makepart::makePart(stack, doc, ball, {}, o), std::invalid_argument);
    // All of it painted: nothing to make a part "over an area" of.
    CHECK_THROWS_AS(makepart::makePart(stack, doc, ball, {{{0, 0, 10}, 50.0, false}}, o), std::invalid_argument);
    o.thickness = 0.0;
    CHECK_THROWS_AS(makepart::makePart(stack, doc, ball, capPaint(doc.worldBounds(ball), 8.0), o),
                    std::invalid_argument);
    CHECK_FALSE((stack.canUndo() && stack.undoLabel().rfind("Make part", 0) == 0));
}
