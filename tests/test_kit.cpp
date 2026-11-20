// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 62: kit layout (sprue).
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Kit.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name, double x, double y)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    b.placement.x = x;
    b.placement.y = y;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

bool apart(const Bounds& a, const Bounds& b, double gap)
{
    return a.max[0] + gap <= b.min[0] + 1e-3 || b.max[0] + gap <= a.min[0] + 1e-3 || a.max[1] + gap <= b.min[1] + 1e-3 ||
           b.max[1] + gap <= a.min[1] + 1e-3;
}

}  // namespace

TEST_CASE("Kit: parts in rows on a frame, gated and numbered", "[kit]")
{
    Document doc;
    CommandStack stack(doc);
    std::vector<ObjectId> ids;
    ids.push_back(add(stack, doc, BoxSpec{12, 8, 4}, "Hand", 0, 0));
    ids.push_back(add(stack, doc, CylinderSpec{3, 10}, "Sword", 0, 0));  // overlapping where they start
    ids.push_back(add(stack, doc, SphereSpec{4}, "Head", 30, 0));
    ids.push_back(add(stack, doc, BoxSpec{6, 6, 2}, "Badge", -30, 10));
    double partsVolume = 0.0;
    for (ObjectId id : ids)
        partsVolume += meshkernel::volume(doc.meshSolid(id));

    kit::Settings s;
    const kit::Plan p = kit::plan(doc, ids, s);
    REQUIRE(p.placed.size() == 4);
    for (std::size_t i = 0; i < p.placed.size(); ++i)
        CHECK(p.placed[i].number == int(i + 1));
    CHECK(p.frame.triangleCount() > 0);

    s.join = false;
    const ObjectId frame = kit::make(stack, doc, ids, s);
    CHECK(stack.undoLabel() == "Make kit");
    // Every part inside the frame, clear of the others; numbered by name.
    const Bounds fb = doc.worldBounds(frame);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const Bounds b = doc.worldBounds(ids[i]);
        CHECK(b.min[0] >= fb.min[0] + s.bar);
        CHECK(b.max[0] <= fb.max[0] - s.bar);
        CHECK(b.min[1] >= fb.min[1] + s.bar);
        CHECK(b.max[1] <= fb.max[1] - s.bar);
        for (std::size_t j = i + 1; j < ids.size(); ++j)
            CHECK(apart(b, doc.worldBounds(ids[j]), s.gap - 0.01));
    }
    const std::string first = doc.find(p.placed.front().id)->name;
    CHECK(first.substr(0, 2) == "1 ");
    CHECK_THAT(fb.min[2], WithinAbs(0.0, 1e-3));

    // Joined: the gates reach into each part (so it's all one piece).
    stack.undo();
    CHECK(doc.find(ids[0])->name == "Hand");
    s.join = true;
    const ObjectId kitId = kit::make(stack, doc, ids, s);
    CHECK(doc.isTopLevel(kitId));
    CHECK_FALSE(doc.isTopLevel(ids[0]));
    const double frameVolume = meshkernel::volume(meshkernel::fromTriangles(p.frame.positions, p.frame.indices));
    const double whole = meshkernel::volume(doc.meshSolid(kitId));
    CHECK(whole < frameVolume + partsVolume);  // the gates overlap the parts
    CHECK(whole > frameVolume + partsVolume - 5.0);
    stack.undo();
    CHECK(doc.isTopLevel(ids[0]));

    // The tree's name on a tag under the frame; the kit named after it.
    s.name = "Hero Hands";
    const kit::Plan named = kit::plan(doc, ids, s);
    CHECK(named.area.min[1] < p.area.min[1] - 3.0);  // the tag hangs under the frame
    const double plainFrame = meshkernel::volume(meshkernel::fromTriangles(p.frame.positions, p.frame.indices));
    CHECK(meshkernel::volume(meshkernel::fromTriangles(named.frame.positions, named.frame.indices)) > plainFrame + 10.0);
    const ObjectId heroes = kit::make(stack, doc, ids, s);
    CHECK(doc.find(heroes)->name == "Hero Hands");
    stack.undo();
    s.includeName = false;
    CHECK(kit::plan(doc, ids, s).area.min[1] == p.area.min[1]);

    kit::Settings bad;
    bad.gate = 5.0;
    CHECK_THROWS_AS(kit::check(bad), std::invalid_argument);
    CHECK_THROWS_AS(kit::plan(doc, {}, kit::Settings{}), std::invalid_argument);
}
