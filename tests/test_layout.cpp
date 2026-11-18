// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 43: laying pieces out for printing.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Layout.h"
#include "forge/core/PrintSplit.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0,
             double rx = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    b.placement.rotationX = rx;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

bool overlap(const Bounds& a, const Bounds& b)
{
    for (int k = 0; k < 2; ++k)
        if (a.max[k] <= b.min[k] + 1e-6 || b.max[k] <= a.min[k] + 1e-6)
            return false;
    return true;
}

}  // namespace

TEST_CASE("Lay out: each piece on its flattest side", "[layout]")
{
    Document doc;
    CommandStack stack(doc);
    const std::array<double, 3> plate{220, 220, 250};

    // A cylinder lying on its side stands up on its flat end.
    const ObjectId can = add(stack, doc, CylinderSpec{15, 15, 60}, 0, 0, 30, 90);
    CHECK(doc.worldBounds(can).size()[2] < 35.0);
    // A box already flat stays as it is.
    const ObjectId box = add(stack, doc, BoxSpec{60, 40, 10}, 100, 0, 0);
    const Placement boxBefore = doc.find(box)->placement;

    LayoutOptions o;
    o.plate = plate;
    const LayoutResult r = layOut(stack, doc, {can, box}, o);
    CHECK(r.plates == 1);
    CHECK_THAT(doc.worldBounds(can).size()[2], WithinAbs(60.0, 0.01));
    CHECK_THAT(doc.worldBounds(can).min[2], WithinAbs(0.0, 0.01));
    CHECK(doc.find(box)->placement.rotationX == boxBefore.rotationX);
    CHECK_THAT(doc.worldBounds(box).size()[2], WithinAbs(10.0, 0.01));
    CHECK(plateHolding(doc.worldBounds(can), plate) == 0);
    CHECK(plateHolding(doc.worldBounds(box), plate) == 0);
    CHECK_FALSE(overlap(doc.worldBounds(can), doc.worldBounds(box)));

    // One undo puts both back.
    stack.undo();
    CHECK(doc.worldBounds(can).size()[2] < 35.0);
    CHECK(doc.find(box)->placement == boxBefore);
}

TEST_CASE("Lay out: split pieces lie flat, never on their pegs", "[layout]")
{
    Document doc;
    CommandStack stack(doc);
    const std::array<double, 3> plate{220, 220, 250};
    const ObjectId tall = add(stack, doc, BoxSpec{40, 40, 300});
    KeyOptions keys;
    const KeyedSplit made = smartSplit(stack, doc, tall, plate, keys);
    REQUIRE(made.pieces.size() == 2);
    LayoutOptions o;
    o.plate = plate;
    SECTION("on their own: lying on a long side, never on the pegs")
    {
        const LayoutResult r = layOut(stack, doc, made.pieces, o);
        CHECK(r.tooBig.empty());
        for (ObjectId p : made.pieces) {
            const Bounds b = doc.worldBounds(p);
            CHECK(plateHolding(b, plate) >= 0);
            CHECK_THAT(b.min[2], WithinAbs(0.0, 0.01));
            CHECK(b.size()[2] <= 40.5);
        }
    }
    SECTION("favouring the cut: standing up, holes down and pegs up")
    {
        o.favour = {{0, 0, 1}};  // the level cut
        const LayoutResult r = layOut(stack, doc, made.pieces, o);
        CHECK(r.tooBig.empty());
        for (ObjectId p : made.pieces) {
            const Bounds b = doc.worldBounds(p);
            CHECK(plateHolding(b, plate) >= 0);
            CHECK_THAT(b.min[2], WithinAbs(0.0, 0.01));
            CHECK(b.size()[0] <= 40.5);  // standing on a whole 40 x 40 face, not its pegs
            CHECK(b.size()[1] <= 40.5);
        }
    }
    CHECK_FALSE(overlap(doc.worldBounds(made.pieces[0]), doc.worldBounds(made.pieces[1])));
}

TEST_CASE("Lay out: as many plates as it takes, nothing too big moved", "[layout]")
{
    Document doc;
    CommandStack stack(doc);
    const std::array<double, 3> plate{220, 220, 250};
    std::vector<ObjectId> tiles;
    for (int i = 0; i < 10; ++i)
        tiles.push_back(add(stack, doc, BoxSpec{100, 100, 20}, 0, 0, i * 25.0));
    const ObjectId huge = add(stack, doc, BoxSpec{300, 300, 10}, 0, -500, 0);
    const Placement hugeBefore = doc.find(huge)->placement;
    std::vector<ObjectId> all = tiles;
    all.push_back(huge);
    LayoutOptions o;
    o.plate = plate;
    const LayoutResult r = layOut(stack, doc, all, o);
    CHECK(r.plates == 3);  // four 100 mm tiles to a 220 mm plate
    REQUIRE(r.tooBig.size() == 1);
    CHECK(r.tooBig.front() == huge);
    CHECK(doc.find(huge)->placement == hugeBefore);
    int onPlate[3] = {0, 0, 0};
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const Bounds b = doc.worldBounds(tiles[i]);
        const int k = plateHolding(b, plate);
        REQUIRE(k >= 0);
        CHECK(k == r.plateOf[i]);
        ++onPlate[k];
        for (std::size_t j = 0; j < i; ++j)
            CHECK_FALSE(overlap(b, doc.worldBounds(tiles[j])));
    }
    CHECK(onPlate[0] == 4);
    CHECK(onPlate[1] == 4);
    CHECK(onPlate[2] == 2);
    // Plates sit side by side along X.
    CHECK_THAT(plateCentreX(2, 220), WithinAbs(480.0, 1e-9));
}

TEST_CASE("A step that fails half way is undone without touching earlier steps", "[command]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{10, 10, 10});  // the step before
    stack.beginMacro("outer");
    stack.beginMacro("inner");
    Placement moved = doc.find(box)->placement;
    moved.x = 50;
    stack.execute(std::make_unique<SetPlacementCommand>(box, moved, false));
    stack.abortMacro();  // the inner step failed: its move is undone
    CHECK(doc.find(box)->placement.x == 0.0);
    stack.endMacro();  // nothing left in the outer one: not kept
    CHECK(stack.undoLabel() == "Add Box");
    // Giving up a macro that had done nothing touches nothing.
    stack.beginMacro("nothing");
    stack.abortMacro();
    CHECK(doc.find(box) != nullptr);
    CHECK(stack.undoLabel() == "Add Box");
}
