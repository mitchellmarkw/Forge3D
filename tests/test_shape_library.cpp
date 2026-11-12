// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 3d: sphere, cone, torus, tube, wedge.
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"
#include "forge/core/Transform.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

constexpr double pi = std::numbers::pi;

double volumeOf(const TopoDS_Shape& s)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props, 1e-7);  // adaptive: accurate on B-spline surfaces too
    return props.Mass();
}

// Area of a cross-section that changes linearly from (ax, ay) to (bx, by) along the
// height, integrated: h * integral of x(t) y(t) dt.
double linearProduct(double ax, double ay, double bx, double by, double h)
{
    return h * (ax * ay + (ax * (by - ay) + ay * (bx - ax)) / 2.0 + (bx - ax) * (by - ay) / 3.0);
}

double ellipsePerimeter(double a, double b)
{
    // Numerical integration; plenty accurate for a volume check.
    const int n = 20000;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = 2.0 * pi * (i + 0.5) / n;
        sum += std::hypot(a * std::sin(t), b * std::cos(t));
    }
    return sum * 2.0 * pi / n;
}

double expectedVolume(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& p) -> double {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
                return p.width * p.depth * p.height;
            else if constexpr (std::is_same_v<T, CylinderSpec>)
                return pi * p.radiusX * p.radiusY * p.height;
            else if constexpr (std::is_same_v<T, SphereSpec>)
                return 4.0 / 3.0 * pi * p.radiusX * p.radiusY * p.radiusZ;
            else if constexpr (std::is_same_v<T, ConeSpec>)
                return pi * linearProduct(p.bottomRadiusX, p.bottomRadiusY, p.topRadiusX, p.topRadiusY, p.height);
            else if constexpr (std::is_same_v<T, TorusSpec>)  // cross-section area x centre-line length
                return pi * p.tubeRadius * p.tubeRadiusZ * ellipsePerimeter(p.ringRadiusX, p.ringRadiusY);
            else if constexpr (std::is_same_v<T, TubeSpec>)
                return pi * (p.outerRadiusX * p.outerRadiusY - p.innerRadiusX * p.innerRadiusY) * p.height;
            else if constexpr (std::is_same_v<T, WedgeSpec>)
                return p.width * p.depth * p.height / 2.0;
            else if constexpr (std::is_same_v<T, PyramidSpec>)
                return linearProduct(p.baseWidth, p.baseDepth, p.topWidth, p.topDepth, p.height);
            else
                return -1.0;  // text (depends on the font) and imported meshes: just above zero
        },
        spec);
}

// Builds, checks it is a valid solid of the right volume, and that its box matches
// halfExtents() with the base on the plate.
void checkShape(const PrimitiveSpec& spec, double volumeTol = 1e-4, double boundsTol = 1e-3)
{
    INFO(typeName(spec));
    const TopoDS_Shape shape = primitives::buildShape(spec);
    REQUIRE_FALSE(shape.IsNull());
    CHECK(BRepCheck_Analyzer(shape).IsValid());
    if (expectedVolume(spec) >= 0.0)
        CHECK_THAT(volumeOf(shape), WithinRel(expectedVolume(spec), volumeTol));
    else
        CHECK(volumeOf(shape) > 0.0);
    const Bounds b = shapeBounds(shape);
    const Vec3 h = halfExtents(spec);
    CHECK_THAT(b.min[0], WithinAbs(-h[0], boundsTol));
    CHECK_THAT(b.max[0], WithinAbs(h[0], boundsTol));
    CHECK_THAT(b.min[1], WithinAbs(-h[1], boundsTol));
    CHECK_THAT(b.max[1], WithinAbs(h[1], boundsTol));
    CHECK_THAT(b.min[2], WithinAbs(0.0, boundsTol));
    CHECK_THAT(b.max[2], WithinAbs(2.0 * h[2], boundsTol));
    CHECK_FALSE(tessellate(shape).empty());
    CHECK_NOTHROW(validate(spec));
}

}  // namespace

TEST_CASE("Every primitive builds a valid solid sitting on the plate", "[shapes]")
{
    for (std::size_t kind = 0; kind < kPrimitiveKindCount; ++kind) {
        const PrimitiveSpec spec = defaultSpec(kind);
        checkShape(spec);
        CHECK_THAT(pivotOf(spec)[2], WithinAbs(halfExtents(spec)[2], 1e-12));
    }
}

TEST_CASE("Names and fields", "[shapes]")
{
    CHECK(typeName(PrimitiveSpec{SphereSpec{}}) == "Sphere");
    CHECK(typeName(PrimitiveSpec{PyramidSpec{}}) == "Pyramid");
    CHECK(fileTypeName(PrimitiveSpec{TorusSpec{}}) == "torus");
    CHECK(specFields(PrimitiveSpec{SphereSpec{}}).size() == 3);
    CHECK(specFields(PrimitiveSpec{TorusSpec{}}).size() == 4);
    CHECK(specFields(PrimitiveSpec{PyramidSpec{}}).size() == 5);

    PrimitiveSpec cone = ConeSpec{};
    setSpecValue(cone, 2, 4.0);
    CHECK(std::get<ConeSpec>(cone).topRadiusX == 4.0);
    const PrimitiveSpec doubled = scaled(ConeSpec{10, 4, 20}, 2.0);
    CHECK(std::get<ConeSpec>(doubled).bottomRadiusY == 20.0);
    CHECK(std::get<ConeSpec>(doubled).topRadiusX == 8.0);
    CHECK(std::get<ConeSpec>(doubled).height == 40.0);
}

TEST_CASE("Round groups", "[shapes]")
{
    CHECK(hasRoundGroups(SphereSpec{}));
    CHECK_FALSE(hasRoundGroups(BoxSpec{}));
    CHECK_FALSE(hasRoundGroups(PyramidSpec{}));
    CHECK(isRound(TubeSpec{10, 7, 20}));
    CHECK_FALSE(isRound(TubeSpec{10, 12, 7, 7, 20}));
    CHECK(isRound(BoxSpec{1, 2, 3}));  // nothing to be oval

    const auto t = std::get<TorusSpec>(madeRound(TorusSpec{12, 15, 3, 5}));
    CHECK(t.ringRadiusY == 12);
    CHECK(t.tubeRadiusZ == 3);
    const auto s = std::get<SphereSpec>(madeRound(SphereSpec{4, 5, 6}));
    CHECK(s.radiusY == 4);
    CHECK(s.radiusZ == 4);
}

TEST_CASE("Oval shapes", "[shapes]")
{
    checkShape(SphereSpec{10, 6, 4}, 1e-4, 1e-3);  // ellipsoid (B-spline surface)
    checkShape(SphereSpec{5, 5, 12}, 1e-4, 1e-3);  // egg standing up
    checkShape(ConeSpec{10, 6, 0, 0, 20});         // oval base, point
    checkShape(ConeSpec{10, 6, 4, 8, 15});         // oval to a differently oval top
    checkShape(ConeSpec{0, 0, 5, 9, 12});          // point down
    checkShape(TubeSpec{15, 10, 12, 6, 8});        // oval pipe, oval hole
    checkShape(TubeSpec{10, 10, 5, 8, 8});         // round outside, oval hole
    checkShape(TorusSpec{12, 12, 3, 5}, 1e-4);     // round ring, oval tube (revolved)
    checkShape(TorusSpec{16, 12, 3, 3}, 1e-3);     // oval ring (swept)
    checkShape(TorusSpec{16, 12, 2, 4}, 1e-3);     // oval ring, oval tube
}

TEST_CASE("Pyramid variants", "[shapes]")
{
    checkShape(PyramidSpec{20, 30, 0, 0, 25});   // point
    checkShape(PyramidSpec{20, 30, 0, 30, 15});  // ridge along Y (a roof)
    checkShape(PyramidSpec{20, 30, 8, 0, 15});   // ridge along X
    checkShape(PyramidSpec{20, 30, 10, 12, 15}); // flat top
    checkShape(PyramidSpec{10, 10, 20, 20, 15}); // upside down: wider top
}

TEST_CASE("Cone variants", "[shapes]")
{
    for (const ConeSpec c : {ConeSpec{0.0, 8.0, 15.0}, ConeSpec{10.0, 4.0, 12.0}, ConeSpec{6.0, 6.0, 10.0}})
        checkShape(c);
    CHECK_THROWS_AS(validate(ConeSpec{0.0, 0.0, 10.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(ConeSpec{-1.0, 2.0, 10.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(ConeSpec{10, 10, 4, 0, 10}), std::invalid_argument);  // half a point
}

TEST_CASE("Invalid sizes are refused", "[shapes]")
{
    CHECK_THROWS_AS(validate(SphereSpec{0.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(TorusSpec{5.0, 5.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(TorusSpec{20, 10, 6, 6}), std::invalid_argument);  // 6 > 10*10/20: hole closes
    CHECK_NOTHROW(validate(TorusSpec{20, 10, 4, 6}));
    CHECK_THROWS_AS(validate(TubeSpec{5.0, 6.0, 10.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(TubeSpec{10, 5, 7, 7, 10}), std::invalid_argument);  // hole sticks out along Y
    CHECK_NOTHROW(validate(PyramidSpec{10, 10, 0, 5, 10}));
    CHECK_THROWS_AS(validate(WedgeSpec{10.0, 0.0, 10.0}), std::invalid_argument);

    Document doc;
    Body b;
    b.id = doc.allocateId();
    b.spec = TubeSpec{5.0, 6.0, 10.0};
    CHECK_THROWS_AS(doc.insertBody(b), std::invalid_argument);
}

TEST_CASE("Face handles resize every primitive", "[shapes]")
{
    const auto resize = [](PrimitiveSpec spec, int axis, int side, double value, ResizeOptions opt = {}) {
        Body b;
        b.spec = spec;
        return resizedFace(b, axis, side, value, opt);
    };

    ResizeOptions oval;
    oval.keepRound = false;

    SECTION("sphere stays a sphere while Round")
    {
        const Body side = resize(SphereSpec{10}, 0, 1, 15);
        CHECK(std::get<SphereSpec>(side.spec) == SphereSpec{15});
        CHECK_THAT(side.placement.z, WithinAbs(0, 1e-9));  // still on the plate
        const Body top = resize(SphereSpec{10}, 2, 1, 30);
        CHECK(std::get<SphereSpec>(top.spec) == SphereSpec{15});
    }
    SECTION("sphere stretches one way when not Round")
    {
        CHECK(std::get<SphereSpec>(resize(SphereSpec{10}, 1, 1, 15, oval).spec) == SphereSpec{10, 15, 10});
        CHECK(std::get<SphereSpec>(resize(SphereSpec{10}, 2, 1, 30, oval).spec) == SphereSpec{10, 10, 15});
    }
    SECTION("cone side scales both ends")
    {
        CHECK(std::get<ConeSpec>(resize(ConeSpec{10, 4, 20}, 0, 1, 20).spec) == ConeSpec{20, 8, 20});
        CHECK(std::get<ConeSpec>(resize(ConeSpec{10, 4, 20}, 0, 1, 20, oval).spec) == ConeSpec{20, 10, 8, 4, 20});
    }
    SECTION("torus")
    {
        const Body wider = resize(TorusSpec{12, 4}, 1, -1, 32);  // outer radius 16 -> 32
        CHECK(std::get<TorusSpec>(wider.spec) == TorusSpec{24, 8});
        const auto t = std::get<TorusSpec>(resize(TorusSpec{12, 4}, 2, 1, 40).spec);  // thickness 8 -> 40
        CHECK(t.tubeRadiusZ == 20);
        CHECK(t.tubeRadius == 20);
        CHECK_NOTHROW(validate(t));
        const auto o = std::get<TorusSpec>(resize(TorusSpec{12, 4}, 0, 1, 24, oval).spec);  // ring X 12 -> 20
        CHECK(o == TorusSpec{20, 12, 4, 4});
        const auto flat = std::get<TorusSpec>(resize(TorusSpec{12, 4}, 2, 1, 2, oval).spec);
        CHECK(flat == TorusSpec{12, 12, 4, 1});
        // Squeezing the ring so far the hole would close grows the ring back.
        const auto squeezed = std::get<TorusSpec>(resize(TorusSpec{12, 4}, 1, 1, 6, oval).spec);
        CHECK_NOTHROW(validate(squeezed));
    }
    SECTION("tube keeps its wall")
    {
        const auto t = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 0, 1, 15).spec);
        CHECK(t == TubeSpec{15, 12, 20});
        const auto small = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 0, 1, 1).spec);
        CHECK_NOTHROW(validate(small));
        const auto o = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 1, 1, 15, oval).spec);
        CHECK(o == TubeSpec{10, 15, 7, 12, 20});
    }
    SECTION("pyramid resizes like a box; base and top keep their proportion")
    {
        const Body b = resize(PyramidSpec{20, 20, 10, 0, 20}, 0, 1, 30);
        CHECK(std::get<PyramidSpec>(b.spec) == PyramidSpec{30, 20, 15, 0, 20});
        CHECK_THAT(b.placement.x, WithinAbs(5, 1e-9));
    }
    SECTION("wedge resizes like a box: the opposite side stays")
    {
        const Body b = resize(WedgeSpec{20, 20, 20}, 0, 1, 30);
        CHECK(std::get<WedgeSpec>(b.spec).width == 30);
        CHECK_THAT(b.placement.x, WithinAbs(5, 1e-9));
    }
    SECTION("uniform")
    {
        ResizeOptions opt;
        opt.uniform = true;
        const auto t = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 2, 1, 40, opt).spec);
        CHECK(t == TubeSpec{20, 14, 40});
    }
}

TEST_CASE("0.6.0 files (format 5) still open", "[shapes][file]")
{
    const std::string json = R"({"format":"forge3d-document","formatVersion":5,"units":"mm","bodies":[
        {"id":1,"name":"S","type":"sphere","parameters":{"radius":7},"placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}},
        {"id":2,"name":"C","type":"cone","parameters":{"radiusBottom":9,"radiusTop":2,"height":11},"placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}},
        {"id":3,"name":"T","type":"torus","parameters":{"majorRadius":12,"minorRadius":3},"placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}},
        {"id":4,"name":"P","type":"tube","parameters":{"outerRadius":8,"innerRadius":6,"height":5},"placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}}]})";
    const auto bodies = file::fromJson(json);
    REQUIRE(bodies.size() == 4);
    CHECK(std::get<SphereSpec>(bodies[0].spec) == SphereSpec{7});
    CHECK(std::get<ConeSpec>(bodies[1].spec) == ConeSpec{9, 2, 11});
    CHECK(std::get<TorusSpec>(bodies[2].spec) == TorusSpec{12, 3});
    CHECK(std::get<TubeSpec>(bodies[3].spec) == TubeSpec{8, 6, 5});
}

TEST_CASE("Every primitive survives save and load", "[shapes][file]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-shapes-" + std::to_string(rd()));
    fs::create_directories(dir);

    Document doc;
    for (std::size_t kind = 0; kind < kPrimitiveKindCount; ++kind) {
        Body b;
        b.id = doc.allocateId();
        b.spec = scaled(defaultSpec(kind), 1.5);
        b.name = typeName(b.spec);
        b.placement.x = 30.0 * static_cast<double>(kind);
        doc.insertBody(b);
    }
    const fs::path path = dir / "shapes.f3d";
    file::save(doc, path, "test");
    const auto bodies = file::load(path);
    REQUIRE(bodies.size() == doc.bodyCount());
    for (std::size_t i = 0; i < bodies.size(); ++i)
        CHECK(bodies[i] == doc.bodies()[i]);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("New primitives combine", "[shapes]")
{
    Document doc;
    const auto add = [&doc](PrimitiveSpec spec, double z) {
        Body b;
        b.id = doc.allocateId();
        b.spec = spec;
        b.placement.z = z;
        doc.insertBody(b);
        return b.id;
    };
    const ObjectId tube = add(TubeSpec{10, 7, 20}, 0);
    const ObjectId ball = add(SphereSpec{9, 9, 5}, 15);
    const TopoDS_Shape merged = doc.previewCombine(BooleanOp::Merge, {tube, ball});
    CHECK(BRepCheck_Analyzer(merged).IsValid());
    CHECK_FALSE(isEmptySolid(merged));
}
