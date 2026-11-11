// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>
#include <vector>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {
Body makeBox(Document& doc, double w = 20, double d = 20, double h = 20)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{w, d, h};
    b.name = doc.uniqueName("Box");
    return b;
}
}  // namespace

TEST_CASE("bodies can be inserted, found and removed", "[document]")
{
    Document doc;
    Body a = makeBox(doc);
    const ObjectId idA = a.id;
    doc.insertBody(a);
    Body b = makeBox(doc);
    const ObjectId idB = b.id;
    doc.insertBody(b);

    REQUIRE(doc.bodyCount() == 2);
    CHECK(doc.find(idA)->name == "Box 1");
    CHECK(doc.find(idB)->name == "Box 2");
    CHECK(doc.indexOf(idB) == 1);

    const Body removed = doc.removeBody(idA);
    CHECK(removed.id == idA);
    CHECK(doc.bodyCount() == 1);
    CHECK(doc.find(idA) == nullptr);
    CHECK(doc.indexOf(idA) == Document::npos);
}

TEST_CASE("unique names reuse the lowest free number", "[document]")
{
    Document doc;
    Body a = makeBox(doc);
    doc.insertBody(a);  // Box 1
    Body b = makeBox(doc);
    doc.insertBody(b);  // Box 2
    doc.removeBody(a.id);
    CHECK(doc.uniqueName("Box") == "Box 1");
    CHECK(doc.uniqueName("Cylinder") == "Cylinder 1");
}

TEST_CASE("invalid input is rejected without changing the document", "[document]")
{
    Document doc;
    Body a = makeBox(doc);
    doc.insertBody(a);

    Body duplicate = a;
    CHECK_THROWS_AS(doc.insertBody(duplicate), std::invalid_argument);

    Body zeroId = makeBox(doc);
    zeroId.id = kInvalidId;
    CHECK_THROWS_AS(doc.insertBody(zeroId), std::invalid_argument);

    Body badSize = makeBox(doc, -5);
    CHECK_THROWS_AS(doc.insertBody(badSize), std::invalid_argument);

    CHECK_THROWS_AS(doc.setSpec(a.id, BoxSpec{0, 1, 1}), std::invalid_argument);
    CHECK_THROWS_AS(doc.removeBody(9999), std::out_of_range);
    CHECK_THROWS_AS(doc.rename(a.id, ""), std::invalid_argument);

    REQUIRE(doc.bodyCount() == 1);
    CHECK(*doc.find(a.id) == a);
}

TEST_CASE("inserting a body with a high id keeps allocateId ahead", "[document]")
{
    Document doc;
    Body b = makeBox(doc);
    b.id = 500;
    doc.insertBody(b);
    CHECK(doc.allocateId() == 501);
}

TEST_CASE("listeners hear about every change", "[document]")
{
    Document doc;
    std::vector<Document::Change> seen;
    const int handle = doc.addListener([&](const Document::Change& c) { seen.push_back(c); });

    Body a = makeBox(doc);
    doc.insertBody(a);
    doc.setPlacement(a.id, Placement{5, 0, 0, 0});
    doc.setPlacement(a.id, Placement{5, 0, 0, 0});  // no change, no event
    doc.rename(a.id, "Lid");
    doc.removeBody(a.id);

    REQUIRE(seen.size() == 4);
    CHECK(seen[0].kind == Document::ChangeKind::Added);
    CHECK(seen[1].kind == Document::ChangeKind::Modified);
    CHECK(seen[2].kind == Document::ChangeKind::Modified);
    CHECK(seen[3].kind == Document::ChangeKind::Removed);
    CHECK(seen[3].id == a.id);

    doc.removeListener(handle);
    doc.insertBody(makeBox(doc));
    CHECK(seen.size() == 4);
}

TEST_CASE("mesh is cached until the spec changes", "[document][geometry]")
{
    Document doc;
    Body a = makeBox(doc, 10, 10, 10);
    doc.insertBody(a);

    const auto first = doc.mesh(a.id);
    CHECK(doc.mesh(a.id) == first);  // same pointer: cached

    doc.setPlacement(a.id, Placement{30, 0, 0, 0});
    CHECK(doc.mesh(a.id) == first);  // moving does not rebuild

    doc.setSpec(a.id, BoxSpec{10, 10, 40});
    const auto rebuilt = doc.mesh(a.id);
    CHECK(rebuilt != first);
    CHECK_THAT(rebuilt->bounds().max[2], WithinAbs(40.0, 1e-4));
    CHECK_THAT(first->bounds().max[2], WithinAbs(10.0, 1e-4));  // old pointer still valid
}

TEST_CASE("world bounds apply the placement", "[document][geometry]")
{
    Document doc;
    Body a = makeBox(doc, 40, 20, 10);
    a.placement = Placement{100, 50, 5, 90};  // quarter turn swaps width and depth
    doc.insertBody(a);

    const Bounds wb = doc.worldBounds(a.id);
    REQUIRE(wb.valid);
    CHECK_THAT(wb.min[0], WithinAbs(90.0, 1e-3));
    CHECK_THAT(wb.max[0], WithinAbs(110.0, 1e-3));
    CHECK_THAT(wb.min[1], WithinAbs(30.0, 1e-3));
    CHECK_THAT(wb.max[1], WithinAbs(70.0, 1e-3));
    CHECK_THAT(wb.min[2], WithinAbs(5.0, 1e-3));
    CHECK_THAT(wb.max[2], WithinAbs(15.0, 1e-3));
}

TEST_CASE("suggestPlacement puts new bodies beside existing ones", "[document]")
{
    Document doc;
    CHECK(suggestPlacement(doc, 20) == Placement{});

    Body a = makeBox(doc, 20, 20, 20);  // spans x = -10..10
    doc.insertBody(a);
    const Placement next = suggestPlacement(doc, 30, 10);
    CHECK_THAT(next.x, WithinAbs(10 + 10 + 15, 1e-3));
    CHECK(next.y == 0.0);
}
