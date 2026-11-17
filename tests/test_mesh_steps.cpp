// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 40: Smooth and Extrude Down.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double z = 0.0)
{
    Body b;
    b.id = doc.allocateId();
    b.name = doc.uniqueName("Shape");
    b.spec = std::move(spec);
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId step(CommandStack& stack, Document& doc, BooleanSpec spec)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(spec.op));
    r.boolean = std::move(spec);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

}  // namespace

TEST_CASE("Smooth: corners rounded, size kept, still closed", "[meshsteps]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    BooleanSpec s;
    s.op = BooleanOp::Smooth;
    s.operands = {box};
    s.smooth = SmoothFinish{3};
    const ObjectId smooth = step(stack, doc, s);
    const auto mesh = doc.mesh(smooth);
    CHECK(mesh->triangleCount() > 1000);  // evened out first
    // Corners pulled in: no point as far out as the box's corners (17.3 from the centre).
    double far = 0.0;
    for (std::size_t i = 0; i + 2 < mesh->positions.size(); i += 3) {
        const double x = mesh->positions[i], y = mesh->positions[i + 1], z = mesh->positions[i + 2] - 10.0;
        far = std::max(far, std::sqrt(x * x + y * y + z * z));
    }
    CHECK(far < 16.5);
    // ...and not shrunk: the volume is kept.
    const double v = volumeOf(doc, smooth);
    CHECK_THAT(v, WithinRel(8000.0, 0.01));
    const auto farthest = [&doc](ObjectId id) {
        const auto m = doc.mesh(id);
        double out = 0.0;
        for (std::size_t i = 0; i + 2 < m->positions.size(); i += 3) {
            const double x = m->positions[i], y = m->positions[i + 1], z = m->positions[i + 2] - 10.0;
            out = std::max(out, std::sqrt(x * x + y * y + z * z));
        }
        return out;
    };

    // Stronger: rounder.
    Body after = *doc.find(smooth);
    after.boolean->smooth->strength = 10;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(smooth), after, "Smooth"));
    CHECK(farthest(smooth) < far - 1.0);
    stack.undo();
    CHECK_THAT(volumeOf(doc, smooth), WithinRel(v, 1e-9));

    // Saved and read back.
    Document again;
    again.replaceAll(file::fromJson(file::toJson(doc, "test")));
    CHECK(again.find(smooth)->boolean->smooth->strength == 3);
    CHECK_THAT(volumeOf(again, smooth), WithinRel(v, 1e-6));

    CHECK_THROWS(step(stack, doc, [&] {
        BooleanSpec bad = s;
        bad.operands = {smooth};
        bad.smooth->strength = 11;
        return bad;
    }()));
}

TEST_CASE("Extrude Down: the underside stretched down to the plate", "[meshsteps]")
{
    Document doc;
    CommandStack stack(doc);
    BooleanSpec e;
    e.op = BooleanOp::ExtrudeDown;
    e.planePoint = {0, 0, 0};
    e.planeNormal = {0, 0, 1};

    SECTION("a box held up in the air becomes a taller box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, 10.0);
        e.operands = {box};
        const ObjectId down = step(stack, doc, e);
        const Bounds b = doc.worldBounds(down);
        CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-3));
        CHECK_THAT(b.max[2], WithinAbs(30.0, 1e-3));
        CHECK_THAT(volumeOf(doc, down), WithinRel(20.0 * 20 * 30, 1e-4));
    }
    SECTION("a ball over the plate gets a column under it")
    {
        SphereSpec ball;  // radius 10
        const ObjectId sphere = add(stack, doc, ball, 5.0);  // bottom 5 mm up
        e.operands = {sphere};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(doc.worldBounds(down).min[2], WithinAbs(0.0, 1e-3));
        // The ball, plus a column 5 mm tall, plus the space under the lower half.
        const double ballVolume = 4.0 / 3.0 * M_PI * 1000.0;
        const double expected = ballVolume / 2 + M_PI * 100.0 * 15.0;  // top half + cylinder r10, 0..15
        CHECK_THAT(volumeOf(doc, down), WithinRel(expected, 0.03));
        // Saved and read back.
        Document again;
        again.replaceAll(file::fromJson(file::toJson(doc, "test")));
        CHECK_THAT(volumeOf(again, down), WithinRel(volumeOf(doc, down), 1e-6));
    }
    SECTION("a T fills in under both arms, and a ring fills in under itself")
    {
        const ObjectId stem = add(stack, doc, BoxSpec{10, 10, 30});
        const ObjectId bar = add(stack, doc, BoxSpec{40, 10, 5}, 30.0);
        BooleanSpec m;
        m.op = BooleanOp::Merge;
        m.operands = {stem, bar};
        const ObjectId t = step(stack, doc, m);
        e.operands = {t};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(volumeOf(doc, down), WithinRel(40.0 * 10 * 35, 1e-4));

        TorusSpec ring;  // lying flat, lifted 3 mm: the hole stays open, the ring gets a skirt
        const ObjectId torus = add(stack, doc, ring, 3.0);
        e.operands = {torus};
        const ObjectId skirt = step(stack, doc, e);
        CHECK_THAT(doc.worldBounds(skirt).min[2], WithinAbs(0.0, 1e-3));
        // The ring's top half, and below its middle a solid annulus (radii 8 and 16) 7 mm tall.
        const double expected = M_PI * M_PI * 12.0 * 16.0 + M_PI * (16.0 * 16.0 - 8.0 * 8.0) * 7.0;
        CHECK_THAT(volumeOf(doc, skirt), WithinRel(expected, 0.03));
    }
    SECTION("a shape sitting on the plate is unchanged")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
        e.operands = {box};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(volumeOf(doc, down), WithinRel(8000.0, 1e-6));
    }
}


