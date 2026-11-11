// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>

using Catch::Matchers::WithinAbs;

namespace {

struct Vec3 {
    float x, y, z;
};

Vec3 vertex(const forge::Mesh& m, std::size_t i)
{
    return {m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
}

// For a convex solid, every triangle's winding normal must point away from the centre.
void requireOutwardWinding(const forge::Mesh& mesh, Vec3 centre)
{
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const Vec3 a = vertex(mesh, mesh.indices[3 * t]);
        const Vec3 b = vertex(mesh, mesh.indices[3 * t + 1]);
        const Vec3 c = vertex(mesh, mesh.indices[3 * t + 2]);
        const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        const Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        const Vec3 n{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        const Vec3 out{(a.x + b.x + c.x) / 3 - centre.x, (a.y + b.y + c.y) / 3 - centre.y,
                       (a.z + b.z + c.z) / 3 - centre.z};
        INFO("triangle " << t);
        REQUIRE(n.x * out.x + n.y * out.y + n.z * out.z > 0.0f);
    }
}

}  // namespace

TEST_CASE("a box tessellates to 12 triangles with matching bounds", "[tessellator]")
{
    const forge::Mesh mesh = forge::tessellate(forge::primitives::makeBox(40.0, 30.0, 20.0));

    CHECK(mesh.triangleCount() == 12);     // 2 per face
    CHECK(mesh.vertexCount() == 24);       // 4 per face, so edges stay sharp
    CHECK(mesh.normals.size() == mesh.positions.size());

    const forge::Bounds b = mesh.bounds();
    REQUIRE(b.valid);
    CHECK_THAT(b.min[0], WithinAbs(-20.0, 1e-4));
    CHECK_THAT(b.max[0], WithinAbs(20.0, 1e-4));
    CHECK_THAT(b.min[1], WithinAbs(-15.0, 1e-4));
    CHECK_THAT(b.max[1], WithinAbs(15.0, 1e-4));
    CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-4));   // sits on the build plate
    CHECK_THAT(b.max[2], WithinAbs(20.0, 1e-4));

    requireOutwardWinding(mesh, {0.0f, 0.0f, 10.0f});
}

TEST_CASE("tessellated normals are unit length and point outward", "[tessellator]")
{
    const forge::Mesh mesh = forge::tessellate(forge::primitives::makeBox(10.0, 10.0, 10.0));
    for (std::size_t i = 0; i < mesh.vertexCount(); ++i) {
        const float nx = mesh.normals[3 * i], ny = mesh.normals[3 * i + 1], nz = mesh.normals[3 * i + 2];
        CHECK_THAT(std::sqrt(nx * nx + ny * ny + nz * nz), WithinAbs(1.0, 1e-5));
        const Vec3 p = vertex(mesh, i);
        CHECK(nx * p.x + ny * p.y + nz * (p.z - 5.0f) > 0.0f);
    }
}

TEST_CASE("a cylinder tessellates finely enough to look round", "[tessellator]")
{
    const forge::Mesh mesh = forge::tessellate(forge::primitives::makeCylinder(10.0, 25.0));

    CHECK(mesh.triangleCount() > 40);
    const forge::Bounds b = mesh.bounds();
    REQUIRE(b.valid);
    CHECK_THAT(b.max[0], WithinAbs(10.0, 0.05));
    CHECK_THAT(b.max[2], WithinAbs(25.0, 1e-4));
    requireOutwardWinding(mesh, {0.0f, 0.0f, 12.5f});
}

TEST_CASE("a null shape gives an empty mesh", "[tessellator]")
{
    const forge::Mesh mesh = forge::tessellate(TopoDS_Shape());
    CHECK(mesh.empty());
    CHECK_FALSE(mesh.bounds().valid);
}

TEST_CASE("an elliptical cylinder tessellates to its outline", "[tessellator][ellipse]")
{
    const forge::Mesh mesh = forge::tessellate(forge::primitives::makeCylinder(20.0, 8.0, 10.0));
    const forge::Bounds b = mesh.bounds();
    REQUIRE(b.valid);
    CHECK_THAT(b.max[0], WithinAbs(20.0, 0.05));
    CHECK_THAT(b.max[1], WithinAbs(8.0, 0.05));
    CHECK_THAT(b.max[2], WithinAbs(10.0, 1e-4));
    requireOutwardWinding(mesh, {0.0f, 0.0f, 5.0f});
}
