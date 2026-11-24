// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 89: Rotate for Best Print.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/PrintOrient.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double z = 0, double rotX = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Part");
    b.placement.z = z;
    b.placement.rotationX = rotX;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

Mesh plateMesh(const Document& doc, ObjectId id)
{
    Mesh m = *doc.mesh(id);
    const Transform w = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        const Vec3 p = w.apply(Vec3{m.positions[i], m.positions[i + 1], m.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            m.positions[i + k] = float(p[k]);
    }
    return m;
}

}  // namespace

TEST_CASE("Print orientation: a stick standing up", "[orient]")
{
    Document doc;
    CommandStack stack(doc);
    // 10 × 5 across, 60 tall.
    const ObjectId stick = add(stack, doc, BoxSpec{10, 5, 60});
    const Mesh m = plateMesh(doc, stick);

    const orient::Facts up = orient::facts(m, {0, 0, -1}, orient::Goal::Fastest);
    CHECK_THAT(up.height, WithinAbs(60.0, 1e-3));
    CHECK_THAT(up.baseArea, WithinAbs(50.0, 1e-3));
    CHECK(up.overhangArea < 1e-6);
    CHECK(up.alongLength > 0.8);

    // Quickest and cleanest: lying on its widest side, 5 mm tall.
    for (const auto goal : {orient::Goal::Fastest, orient::Goal::Quality, orient::Goal::Strongest}) {
        INFO(orient::title(goal));
        const orient::Choice c = orient::best(m, goal);
        CHECK_THAT(c.now.height, WithinAbs(60.0, 1e-3));
        CHECK(c.best.alongLength < 0.05);  // lying along the plate
        CHECK(c.best.overhangArea < 1.0);
        CHECK(c.best.score < c.now.score);
        if (goal != orient::Goal::Strongest)
            CHECK_THAT(c.best.height, WithinAbs(5.0, 1e-3));
    }

    // Put that way: lying down, resting on the plate.
    orient::Choice c;
    const Placement p = orient::bestPlacement(doc, stick, orient::Goal::Fastest, &c);
    Body after = *doc.find(stick);
    after.placement = p;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(stick), after, "Rotate for Best Print"));
    const Bounds b = doc.worldBounds(stick);
    CHECK_THAT(b.max[2] - b.min[2], WithinAbs(5.0, 1e-3));
    CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-3));
    // Already best: left as it is.
    const orient::Choice again = orient::best(plateMesh(doc, stick), orient::Goal::Fastest);
    CHECK_THAT(again.best.down[2], WithinAbs(-1.0, 1e-9));
}

TEST_CASE("Print orientation: a mushroom prints cap down", "[orient]")
{
    Document doc;
    CommandStack stack(doc);
    // A thin stem with a wide cap on top: upright, the cap's underside needs support.
    const ObjectId stem = add(stack, doc, CylinderSpec{3, 20});
    const ObjectId cap = add(stack, doc, CylinderSpec{12, 3}, 20.0);
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Mushroom";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = {stem, cap};
    merged.boolean = spec;
    const ObjectId id = merged.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(merged)));
    const Mesh m = plateMesh(doc, id);

    const orient::Facts now = orient::facts(m, {0, 0, -1}, orient::Goal::Fastest);
    CHECK(now.overhangArea > 300.0);  // the cap's underside
    CHECK(now.supportVolume > 5000.0);
    const orient::Choice fast = orient::best(m, orient::Goal::Fastest);
    CHECK(fast.best.down[2] > 0.99);  // upside down: on the cap
    CHECK(fast.best.overhangArea < 1.0);
    CHECK(fast.best.baseArea > 400.0);
    const orient::Choice clean = orient::best(m, orient::Goal::Quality);
    CHECK(clean.best.down[2] > 0.99);
}

TEST_CASE("Print orientation: a turned part is put right", "[orient]")
{
    Document doc;
    CommandStack stack(doc);
    // A plate 40 × 30 × 4, tipped 30° so it rests on an edge.
    const ObjectId plate = add(stack, doc, BoxSpec{40, 30, 4}, 10.0, 30.0);
    orient::Choice c;
    const Placement p = orient::bestPlacement(doc, plate, orient::Goal::Quality, &c);
    CHECK(c.now.overhangArea > 100.0);
    Body after = *doc.find(plate);
    after.placement = p;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(plate), after, "Rotate for Best Print"));
    const Bounds b = doc.worldBounds(plate);
    CHECK_THAT(b.max[2] - b.min[2], WithinAbs(4.0, 1e-3));
    CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-3));
}

// Slice 92: resin printers, and turning to fit.
TEST_CASE("Print orientation: the biggest layer", "[orient]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId stick = add(stack, doc, BoxSpec{10, 5, 60});
    const Mesh m = plateMesh(doc, stick);
    CHECK_THAT(orient::facts(m, {0, 0, -1}, orient::Goal::Quality).largestLayer, WithinAbs(50.0, 1.0));
    CHECK_THAT(orient::facts(m, {0, 1, 0}, orient::Goal::Quality).largestLayer, WithinAbs(600.0, 6.0));
}

TEST_CASE("Print orientation: resin", "[orient]")
{
    Document doc;
    CommandStack stack(doc);
    // A flat plate 40 × 30 × 4, lying flat.
    const ObjectId plate = add(stack, doc, BoxSpec{40, 30, 4});
    const Mesh m = plateMesh(doc, plate);
    orient::Settings resin;
    resin.printer = orient::Printer::Resin;
    // Filament: lying flat is the cleanest (no supports, no slopes).
    const orient::Choice fdm = orient::best(m, orient::Goal::Quality);
    CHECK(fdm.best.down[2] < -0.99);
    // Resin: lying flat, every layer is the whole plate pulling on the film. Stood up or
    // tipped, the layers are small.
    const orient::Choice r = orient::best(m, orient::Goal::Quality, {0, 0, -1}, resin);
    CHECK(r.best.down[2] > -0.9);
    CHECK(r.best.largestLayer < 0.3 * r.now.largestLayer);
    CHECK_THAT(r.now.largestLayer, WithinAbs(1200.0, 12.0));
    // Resin's time is its height: flat is quickest.
    const orient::Choice quick = orient::best(m, orient::Goal::Fastest, {0, 0, -1}, resin);
    CHECK_THAT(quick.best.height, WithinAbs(4.0, 1e-3));
}

TEST_CASE("Print orientation: turned to take the least room", "[orient]")
{
    // A 40 × 10 outline turned 30°: turned back, its long side along X.
    std::vector<std::array<double, 2>> pts;
    const double a = 30.0 * std::numbers::pi / 180.0;
    for (const auto& [x, y] : std::vector<std::pair<double, double>>{{-20, -5}, {20, -5}, {20, 5}, {-20, 5}, {0, 0}})
        pts.push_back({std::cos(a) * x - std::sin(a) * y, std::sin(a) * x + std::cos(a) * y});
    const double turn = orient::turnToFit(pts);
    CHECK_THAT(std::abs(turn), WithinAbs(30.0, 1e-6));
    // Already square to the axes, long side along X: left as it is.
    CHECK_THAT(orient::turnToFit({{-20, -5}, {20, -5}, {20, 5}, {-20, 5}}), WithinAbs(0.0, 1e-9));
    // Long side along Y: a quarter turn.
    CHECK_THAT(std::abs(orient::turnToFit({{-5, -20}, {5, -20}, {5, 20}, {-5, 20}})), WithinAbs(90.0, 1e-6));

    // On the plate: a box turned 30° comes out square to the plate, long side along X.
    Document doc;
    CommandStack stack(doc);
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{10, 40, 6};
    b.name = "Box";
    b.placement.rotationZ = 30.0;
    const ObjectId box = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    const Placement p = orient::bestPlacement(doc, box, orient::Goal::Fastest);
    Body after = *doc.find(box);
    after.placement = p;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box), after, "Rotate for Best Print"));
    const Bounds bb = doc.worldBounds(box);
    CHECK_THAT(bb.max[0] - bb.min[0], WithinAbs(40.0, 1e-3));
    CHECK_THAT(bb.max[1] - bb.min[1], WithinAbs(10.0, 1e-3));
    CHECK_THAT(bb.max[2] - bb.min[2], WithinAbs(6.0, 1e-3));
    // Asked not to: only laid down.
    orient::Settings keep;
    keep.turnToFit = false;
    Body untouched = *doc.find(box);
    untouched.placement.rotationZ = 30.0;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box), untouched, "Turn"));
    const Placement q = orient::bestPlacement(doc, box, orient::Goal::Fastest, nullptr, keep);
    CHECK_THAT(q.rotationZ, WithinAbs(doc.find(box)->placement.rotationZ, 1e-6));
}
