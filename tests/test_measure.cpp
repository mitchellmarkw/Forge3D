// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 25: measuring.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// A 20 mm cube with two holes, 3 mm across, at x = -5 and x = +5, down through it.
struct Holed {
    Document doc;
    CommandStack stack{doc};
    ObjectId result = kInvalidId;
    Holed()
    {
        Body box;
        box.id = doc.allocateId();
        box.name = "Box";
        box.spec = BoxSpec{20, 20, 20};
        stack.execute(std::make_unique<AddBodyCommand>(box));
        std::vector<ObjectId> parts{box.id};
        for (double x : {-5.0, 5.0}) {
            Body hole;
            hole.id = doc.allocateId();
            hole.name = "Hole";
            hole.spec = CylinderSpec{1.5, 1.5, 30};
            hole.placement.x = x;
            hole.placement.z = -5;
            stack.execute(std::make_unique<AddBodyCommand>(hole));
            parts.push_back(hole.id);
        }
        Body cut;
        cut.id = doc.allocateId();
        cut.name = "Cut";
        BooleanSpec spec;
        spec.op = BooleanOp::Subtract;
        spec.operands = parts;
        cut.boolean = spec;
        stack.execute(std::make_unique<CombineCommand>(cut));
        result = cut.id;
    }
};

int edgeWhere(const measure::Shape& s, const std::function<bool(const std::vector<Vec3>&)>& test)
{
    for (std::size_t i = 0; i < s.edgeLines().size(); ++i)
        if (test(s.edgeLines()[i]))
            return int(i);
    return -1;
}

}  // namespace

TEST_CASE("Measure: what faces and edges are", "[measure]")
{
    Holed h;
    const measure::Shape shape(pickableShape(h.doc, h.result));
    const auto top = shape.faceUnder({0, 8, 50}, {0, 0, -1});
    REQUIRE(top);
    const double zTop = top->point[2];
    const auto about = measure::describe(shape.face(top->face, top->point));
    CHECK(about.kind == measure::About::Kind::Flat);
    CHECK_THAT(about.area, WithinRel(400.0 - 2 * std::numbers::pi * 1.5 * 1.5, 1e-6));
    CHECK_THAT(about.direction[2], WithinAbs(1.0, 1e-9));

    // The hole's wall, from inside the hole.
    const auto wall = shape.faceUnder({5, 0, zTop - 10}, {1, 0, 0});
    REQUIRE(wall);
    const auto hole = measure::describe(shape.face(wall->face, wall->point));
    CHECK(hole.kind == measure::About::Kind::Cylinder);
    CHECK_THAT(hole.radius, WithinAbs(1.5, 1e-9));

    // The hole's rim on top: a circle; an edge of the cube: a line 20 long.
    const int rim = edgeWhere(shape, [&](const std::vector<Vec3>& l) {
        return std::all_of(l.begin(), l.end(), [&](const Vec3& p) {
            return std::abs(p[2] - zTop) < 1e-6 && std::abs(std::hypot(p[0] - 5, p[1]) - 1.5) < 1e-3;
        });
    });
    REQUIRE(rim >= 0);
    const auto circle = measure::describe(shape.edge(rim, shape.edgeLines()[std::size_t(rim)].front()));
    CHECK(circle.kind == measure::About::Kind::Circle);
    CHECK_THAT(circle.radius, WithinAbs(1.5, 1e-9));
    CHECK_THAT(circle.length, WithinRel(2 * std::numbers::pi * 1.5, 1e-6));
    const int side = edgeWhere(shape, [&](const std::vector<Vec3>& l) {
        return l.size() == 2 && std::abs(l[0][2] - l[1][2]) > 19;
    });
    REQUIRE(side >= 0);
    const auto line = measure::describe(shape.edge(side, shape.edgeLines()[std::size_t(side)].front()));
    CHECK(line.kind == measure::About::Kind::Line);
    CHECK_THAT(line.length, WithinAbs(20.0, 1e-9));
    CHECK(shape.corners().size() >= 8);
    CHECK(!shape.faceMesh(top->face).empty());
}

TEST_CASE("Measure: distances, angles and centres between two things", "[measure]")
{
    Holed h;
    const measure::Shape shape(pickableShape(h.doc, h.result));
    const auto topHit = shape.faceUnder({0, 8, 50}, {0, 0, -1});
    const auto bottomHit = shape.faceUnder({0, 8, -50}, {0, 0, 1});
    const auto sideHit = shape.faceUnder({50, 8, 10}, {-1, 0, 0});
    REQUIRE((topHit && bottomHit && sideHit));
    const auto top = shape.face(topHit->face, topHit->point);
    const auto bottom = shape.face(bottomHit->face, bottomHit->point);
    const auto sideFace = shape.face(sideHit->face, sideHit->point);

    const auto across = measure::between(top, bottom);
    CHECK(across.parallel);
    REQUIRE(across.apart);
    CHECK_THAT(*across.apart, WithinAbs(20.0, 1e-9));
    CHECK_THAT(across.distance, WithinAbs(20.0, 1e-6));

    const auto corner = measure::between(top, sideFace);
    REQUIRE(corner.angle);
    CHECK_THAT(*corner.angle, WithinAbs(90.0, 1e-9));
    CHECK_THAT(corner.distance, WithinAbs(0.0, 1e-6));  // they touch

    // Between the two holes' walls: their axes 10 apart, the walls 7.
    const auto wallA = shape.faceUnder({-5, 0, 10}, {1, 0, 0});
    const auto wallB = shape.faceUnder({5, 0, 10}, {1, 0, 0});
    REQUIRE((wallA && wallB));
    const auto holes = measure::between(shape.face(wallA->face, wallA->point), shape.face(wallB->face, wallB->point));
    REQUIRE(holes.centres);
    CHECK_THAT(*holes.centres, WithinAbs(10.0, 1e-9));
    CHECK_THAT(holes.distance, WithinAbs(7.0, 1e-6));

    // A hole and the side of the cube: the hole's axis 5 mm in from it (x = 5 to 10).
    const auto holeToSide = measure::between(shape.face(wallB->face, wallB->point), sideFace);
    REQUIRE(holeToSide.centreToFace);
    CHECK_THAT(*holeToSide.centreToFace, WithinAbs(5.0, 1e-9));
    CHECK(!holeToSide.apart);  // not "parallel, n mm apart": a round face has no one place

    // Two points: straight across, and how far along each axis.
    const measure::Target p{measure::Target::Kind::Point, {}, {0, 0, 0}};
    const measure::Target q{measure::Target::Kind::Point, {}, {3, 4, 12}};
    const auto pq = measure::between(p, q);
    CHECK_THAT(pq.distance, WithinAbs(13.0, 1e-9));
    CHECK(!pq.angle);

    // A point to a face: straight to it.
    const auto above = measure::between({measure::Target::Kind::Point, {}, {0, 8, 30}}, top);
    CHECK_THAT(above.distance, WithinAbs(30.0 - topHit->point[2], 1e-6));
}
