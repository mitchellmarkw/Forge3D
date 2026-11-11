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
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
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
                return 4.0 / 3.0 * pi * p.radius * p.radius * p.radius;
            else if constexpr (std::is_same_v<T, ConeSpec>)
                return pi * p.height / 3.0 *
                       (p.radiusBottom * p.radiusBottom + p.radiusBottom * p.radiusTop + p.radiusTop * p.radiusTop);
            else if constexpr (std::is_same_v<T, TorusSpec>)
                return 2.0 * pi * pi * p.majorRadius * p.minorRadius * p.minorRadius;
            else if constexpr (std::is_same_v<T, TubeSpec>)
                return pi * (p.outerRadius * p.outerRadius - p.innerRadius * p.innerRadius) * p.height;
            else
                return p.width * p.depth * p.height / 2.0;
        },
        spec);
}

}  // namespace

TEST_CASE("Every primitive builds a valid solid sitting on the plate", "[shapes]")
{
    for (std::size_t kind = 0; kind < kPrimitiveKindCount; ++kind) {
        const PrimitiveSpec spec = defaultSpec(kind);
        INFO(typeName(spec));
        const TopoDS_Shape shape = primitives::buildShape(spec);
        REQUIRE_FALSE(shape.IsNull());
        CHECK(BRepCheck_Analyzer(shape).IsValid());
        CHECK_THAT(volumeOf(shape), WithinRel(expectedVolume(spec), 1e-6));

        // Bounding box: centred on X / Y, base on the plate, size from halfExtents().
        const Bounds b = shapeBounds(shape);
        const Vec3 h = halfExtents(spec);
        CHECK_THAT(b.min[0], WithinAbs(-h[0], 1e-3));
        CHECK_THAT(b.max[0], WithinAbs(h[0], 1e-3));
        CHECK_THAT(b.min[1], WithinAbs(-h[1], 1e-3));
        CHECK_THAT(b.max[1], WithinAbs(h[1], 1e-3));
        CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-3));
        CHECK_THAT(b.max[2], WithinAbs(2.0 * h[2], 1e-3));
        CHECK_THAT(pivotOf(spec)[2], WithinAbs(h[2], 1e-12));

        CHECK_FALSE(tessellate(shape).empty());
        CHECK_NOTHROW(validate(spec));
    }
}

TEST_CASE("Names and fields", "[shapes]")
{
    CHECK(typeName(PrimitiveSpec{SphereSpec{}}) == "Sphere");
    CHECK(typeName(PrimitiveSpec{WedgeSpec{}}) == "Wedge");
    CHECK(fileTypeName(PrimitiveSpec{TorusSpec{}}) == "torus");
    CHECK(specFields(PrimitiveSpec{SphereSpec{}}).size() == 1);
    CHECK(specFields(PrimitiveSpec{TorusSpec{}}).size() == 2);

    PrimitiveSpec cone = ConeSpec{};
    setSpecValue(cone, 1, 4.0);
    CHECK(std::get<ConeSpec>(cone).radiusTop == 4.0);
    const PrimitiveSpec doubled = scaled(cone, 2.0);
    CHECK(std::get<ConeSpec>(doubled).radiusBottom == 20.0);
    CHECK(std::get<ConeSpec>(doubled).radiusTop == 8.0);
    CHECK(std::get<ConeSpec>(doubled).height == 40.0);
}

TEST_CASE("Cone variants", "[shapes]")
{
    // Point down, cut-off, and equal radii (becomes a cylinder).
    for (const ConeSpec c : {ConeSpec{0.0, 8.0, 15.0}, ConeSpec{10.0, 4.0, 12.0}, ConeSpec{6.0, 6.0, 10.0}}) {
        const TopoDS_Shape s = primitives::buildShape(c);
        CHECK(BRepCheck_Analyzer(s).IsValid());
        CHECK_THAT(volumeOf(s), WithinRel(expectedVolume(c), 1e-6));
    }
    CHECK_THROWS_AS(validate(ConeSpec{0.0, 0.0, 10.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(ConeSpec{-1.0, 2.0, 10.0}), std::invalid_argument);
}

TEST_CASE("Invalid sizes are refused", "[shapes]")
{
    CHECK_THROWS_AS(validate(SphereSpec{0.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(TorusSpec{5.0, 5.0}), std::invalid_argument);
    CHECK_THROWS_AS(validate(TubeSpec{5.0, 6.0, 10.0}), std::invalid_argument);
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

    SECTION("sphere stays a sphere")
    {
        const Body side = resize(SphereSpec{10}, 0, 1, 15);
        CHECK(std::get<SphereSpec>(side.spec).radius == 15);
        CHECK_THAT(side.placement.z, WithinAbs(0, 1e-9));  // still on the plate
        const Body top = resize(SphereSpec{10}, 2, 1, 30);
        CHECK(std::get<SphereSpec>(top.spec).radius == 15);
    }
    SECTION("cone side scales both radii")
    {
        const Body b = resize(ConeSpec{10, 4, 20}, 0, 1, 20);
        CHECK(std::get<ConeSpec>(b.spec).radiusBottom == 20);
        CHECK(std::get<ConeSpec>(b.spec).radiusTop == 8);
        CHECK(std::get<ConeSpec>(b.spec).height == 20);
    }
    SECTION("torus")
    {
        const Body wider = resize(TorusSpec{12, 4}, 1, -1, 32);  // outer radius 16 -> 32
        CHECK_THAT(std::get<TorusSpec>(wider.spec).majorRadius, WithinAbs(24, 1e-9));
        CHECK_THAT(std::get<TorusSpec>(wider.spec).minorRadius, WithinAbs(8, 1e-9));
        const Body thick = resize(TorusSpec{12, 4}, 2, 1, 40);  // thickness 8 -> 40
        const auto t = std::get<TorusSpec>(thick.spec);
        CHECK(t.minorRadius == 20);
        CHECK(t.majorRadius > t.minorRadius);
        CHECK_NOTHROW(validate(thick.spec));
    }
    SECTION("tube keeps its wall")
    {
        const auto t = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 0, 1, 15).spec);
        CHECK(t.outerRadius == 15);
        CHECK(t.innerRadius == 12);
        const auto small = std::get<TubeSpec>(resize(TubeSpec{10, 7, 20}, 0, 1, 1).spec);
        CHECK_NOTHROW(validate(small));
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
        CHECK(t.outerRadius == 20);
        CHECK(t.innerRadius == 14);
    }
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
    const ObjectId ball = add(SphereSpec{9}, 6);
    const TopoDS_Shape merged = doc.previewCombine(BooleanOp::Merge, {tube, ball});
    CHECK(BRepCheck_Analyzer(merged).IsValid());
    CHECK_FALSE(isEmptySolid(merged));
}
