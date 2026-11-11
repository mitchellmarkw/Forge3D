// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"
#include "forge/core/Picking.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {
ObjectId addBox(Document& doc, double size, Placement p)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{size, size, size};
    b.name = doc.uniqueName("Box");
    b.placement = p;
    doc.insertBody(b);
    return b.id;
}

Ray straightDown(double x, double y)
{
    return Ray{{x, y, 500.0}, {0.0, 0.0, -1.0}};
}
}  // namespace

TEST_CASE("a ray from above hits the top face", "[picking]")
{
    Document doc;
    const ObjectId id = addBox(doc, 20, {});
    const auto hit = pick(doc, straightDown(0, 0));
    REQUIRE(hit);
    CHECK(hit->id == id);
    CHECK_THAT(hit->point[2], WithinAbs(20.0, 1e-6));
    CHECK_THAT(hit->distance, WithinAbs(480.0, 1e-6));
}

TEST_CASE("a ray that misses everything returns nothing", "[picking]")
{
    Document doc;
    addBox(doc, 20, {});
    CHECK_FALSE(pick(doc, straightDown(50, 50)));
    CHECK_FALSE(pick(Document(), straightDown(0, 0)));
}

TEST_CASE("the nearest body wins", "[picking]")
{
    Document doc;
    const ObjectId low = addBox(doc, 10, {});
    const ObjectId high = addBox(doc, 10, Placement{0, 0, 30, 0});  // stacked above
    const auto hit = pick(doc, straightDown(0, 0));
    REQUIRE(hit);
    CHECK(hit->id == high);

    const auto fromBelow = pick(doc, Ray{{0, 0, -100}, {0, 0, 1}});
    REQUIRE(fromBelow);
    CHECK(fromBelow->id == low);
}

TEST_CASE("placement and rotation are respected", "[picking]")
{
    Document doc;
    // 40 x 10 box turned a quarter: it now runs along Y, centred at (100, 0).
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{40, 10, 10};
    b.placement = Placement{100, 0, 0, 90};
    b.name = "Bar";
    doc.insertBody(b);

    CHECK(pick(doc, straightDown(100, 15)));         // inside after rotation
    CHECK_FALSE(pick(doc, straightDown(115, 0)));    // would be inside without rotation
    CHECK_FALSE(pick(doc, straightDown(0, 0)));      // where it would be without placement
}

TEST_CASE("a ray pointing away does not hit", "[picking]")
{
    Document doc;
    addBox(doc, 20, {});
    CHECK_FALSE(pick(doc, Ray{{0, 0, 500}, {0, 0, 1}}));
    CHECK_FALSE(pick(doc, Ray{{0, 0, 500}, {0, 0, 0}}));  // zero direction
}
