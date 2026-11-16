// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 26: screw threads.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"
#include "forge/core/Shell.h"
#include "forge/core/Thread.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// The face of `id` a ray from `origin` along `dir` meets first, as a FaceRef.
FaceRef faceAt(const Document& doc, ObjectId id, const Vec3& origin, const Vec3& dir)
{
    const TopoDS_Shape shape = pickableShape(doc, id);
    const edges::FaceFinder finder(shape);
    const auto hit = finder.under(origin, dir);
    REQUIRE(hit);
    return shell::refFor(shape, hit->face, hit->point);
}

ObjectId add(Document& doc, CommandStack& stack, PrimitiveSpec spec, const char* name, double x = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.name = name;
    b.spec = spec;
    b.placement.x = x;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

}  // namespace

TEST_CASE("Threads: sizes, and the round face found", "[thread]")
{
    const auto& sizes = thread::metric();
    REQUIRE(sizes.size() >= 10);
    CHECK(sizes.front().name == "M2");

    Document doc;
    CommandStack stack(doc);
    const ObjectId peg = add(doc, stack, CylinderSpec{3, 3, 12}, "Peg");
    const TopoDS_Shape shape = pickableShape(doc, peg);
    const edges::FaceFinder finder(shape);
    const auto side = finder.under({20, 0, 6}, {-1, 0, 0});
    REQUIRE(side);
    const auto round = thread::roundFace(shape, side->face);
    REQUIRE(round);
    CHECK(!round->hole);
    CHECK_THAT(round->radius, WithinAbs(3.0, 1e-9));
    CHECK_THAT(round->to - round->from, WithinAbs(12.0, 1e-9));
    CHECK(thread::suggest(*round).name == "M6");
    const auto top = finder.under({0, 0, 50}, {0, 0, -1});
    REQUIRE(top);
    CHECK(!thread::roundFace(shape, top->face));  // flat: no thread
}

TEST_CASE("Threads: an M6 peg and an M6 hole", "[thread]")
{
    Document doc;
    CommandStack stack(doc);

    SECTION("A peg: cut down to the thread, closed, its threads the right pitch")
    {
        const ObjectId peg = add(doc, stack, CylinderSpec{3, 3, 12}, "Peg");
        ThreadFinish f;
        f.face = faceAt(doc, peg, {20, 0, 6}, {-1, 0, 0});
        f.size = 6;
        f.pitch = 1;
        f.fit = 0.2;
        Body step;
        step.id = doc.allocateId();
        step.name = "Threaded";
        step.boolean = threadSpec(doc, peg, f);
        stack.execute(std::make_unique<CombineCommand>(step));
        const double before = std::numbers::pi * 9 * 12;
        const double after = meshkernel::volume(doc.meshSolid(step.id));
        CHECK(after < before * 0.9);   // grooves cut
        CHECK(after > before * 0.6);   // but most of it left
        const Bounds b = doc.worldBounds(step.id);
        CHECK_THAT(b.size()[2], WithinAbs(12.0, 1e-3));  // not taller or shorter
        CHECK(b.size()[0] <= 6.0 - 2 * 0.2 + 0.02);       // the fit taken off all round
        CHECK(history::describe(doc, step.id).find("M6 thread") != std::string::npos);

        // Down the side, the surface goes in and out once per millimetre: a ray at
        // each height meets it at the crest or in the groove.
        const Mesh& m = *doc.mesh(step.id);
        int changes = 0;
        bool wasIn = false;
        for (double z = 1.0; z <= 11.0; z += 0.05) {
            double outside = 0;
            for (std::size_t v = 0; v < m.vertexCount(); ++v)
                if (std::abs(m.positions[3 * v + 2] - z) < 0.03 && std::abs(m.positions[3 * v + 1]) < 0.15 &&
                    m.positions[3 * v] > 0)
                    outside = std::max(outside, double(m.positions[3 * v]));
            if (outside == 0)
                continue;
            const bool in = outside < 2.5;
            if (in != wasIn)
                ++changes;
            wasIn = in;
        }
        CHECK(changes >= 16);  // about 10 turns: in and out each
        CHECK(changes <= 24);

        // Saved and opened again: the same.
        const auto loaded = file::fromJson(file::toJson(doc, "test"));
        Document other;
        other.replaceAll(loaded);
        CHECK(*other.find(step.id) == *doc.find(step.id));
        CHECK_THAT(meshkernel::volume(other.meshSolid(step.id)), WithinRel(after, 1e-9));

        // A longer peg: the thread runs the new length.
        Body longer = *doc.find(peg);
        longer.spec = CylinderSpec{3, 3, 20};
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(peg), longer, "Longer"));
        CHECK_THAT(doc.worldBounds(step.id).size()[2], WithinAbs(20.0, 1e-3));
        CHECK(meshkernel::volume(doc.meshSolid(step.id)) > after * 1.5);

        // Resized evenly: the thread grows with it.
        for (const Body& x : scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0}))
            if (x.id == step.id) {
                CHECK_THAT(x.boolean->thread->size, WithinAbs(12.0, 1e-9));
                CHECK_THAT(x.boolean->thread->pitch, WithinAbs(2.0, 1e-9));
            }
    }

    SECTION("A hole: opened to the thread's inside, grooves cut outwards, the block outside untouched")
    {
        const ObjectId block = add(doc, stack, BoxSpec{20, 20, 10}, "Block");
        const ObjectId drill = add(doc, stack, CylinderSpec{2.5, 2.5, 20}, "Drill", 0, -5);
        Body cut;
        cut.id = doc.allocateId();
        cut.name = "Drilled";
        BooleanSpec sub;
        sub.op = BooleanOp::Subtract;
        sub.operands = {block, drill};
        cut.boolean = sub;
        stack.execute(std::make_unique<CombineCommand>(cut));
        const double drilled = 20.0 * 20.0 * 10.0 - std::numbers::pi * 2.5 * 2.5 * 10.0;
        ThreadFinish f;
        f.face = faceAt(doc, cut.id, {0, 0, 5}, {1, 0, 0});  // from inside the hole
        f.size = 6;
        f.pitch = 1;
        f.fit = 0.2;
        const auto round = thread::roundFace(pickableShape(doc, cut.id), [&] {
            const edges::FaceFinder finder(pickableShape(doc, cut.id));
            return finder.under({0, 0, 5}, {1, 0, 0})->face;
        }());
        REQUIRE(round);
        CHECK(round->hole);
        CHECK(thread::suggest(*round).name == "M6");
        Body step;
        step.id = doc.allocateId();
        step.name = "Threaded";
        step.boolean = threadSpec(doc, cut.id, f);
        stack.execute(std::make_unique<CombineCommand>(step));
        const double after = meshkernel::volume(doc.meshSolid(step.id));
        CHECK(after < drilled - 5.0);
        CHECK(after > drilled - 120.0);  // about 25 opening it up, 60 of groove
        const Bounds b = doc.worldBounds(step.id);
        CHECK_THAT(b.size()[0], WithinAbs(20.0, 1e-3));
        CHECK_THAT(b.size()[2], WithinAbs(10.0, 1e-3));
    }

    SECTION("Left-hand threads turn the other way; a mesh can't be threaded")
    {
        const ObjectId peg = add(doc, stack, CylinderSpec{3, 3, 12}, "Peg");
        const auto face = faceAt(doc, peg, {20, 0, 6}, {-1, 0, 0});
        thread::Round round = *thread::roundFace(pickableShape(doc, peg), [&] {
            const edges::FaceFinder finder(pickableShape(doc, peg));
            return finder.under({20, 0, 6}, {-1, 0, 0})->face;
        }());
        ThreadFinish right;
        right.face = face;
        ThreadFinish left = right;
        left.leftHand = true;
        const auto a = thread::cutFor(round, right).groove;
        const auto b = thread::cutFor(round, left).groove;
        CHECK(a.positions != b.positions);
        // Mirrored left to right, a left-hand groove is a right-hand one.
        Mesh flipped = b;
        for (std::size_t v = 0; v < flipped.vertexCount(); ++v)
            flipped.positions[3 * v + 1] = -flipped.positions[3 * v + 1];
        CHECK_THAT(flipped.bounds().max[1], WithinAbs(a.bounds().max[1], 0.05));
        ThreadFinish tooDeep = right;
        tooDeep.pitch = 6;
        CHECK_THROWS(thread::cutFor(round, tooDeep));
    }
}
