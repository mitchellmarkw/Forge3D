// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 54: magnet pockets.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Pose.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId addBox(CommandStack& stack, Document& doc, double z, const std::string& name)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{20, 20, 20};
    b.name = name;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId addPocket(CommandStack& stack, Document& doc, ObjectId target, const emboss::SurfacePoint& at,
                   const SocketFinish& f)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Pocketed");
    r.boolean = socketSpec(doc, target, at, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

SocketFinish magnet(double cover = 0.0)
{
    SocketFinish f;
    f.kind = SocketKind::Magnet;
    f.size = 6.0;
    f.depth = 2.0;
    f.fit = 0.1;
    f.cover = cover;
    return f;
}

const emboss::SurfacePoint kTop{{0, 0, 20}, {0, 0, 1}};

}  // namespace

TEST_CASE("Magnet: an open pocket the magnet sits flush in", "[magnet]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 0, "Head");
    const ObjectId pocket = addPocket(stack, doc, box, kTop, magnet());
    const double removed = 8000.0 - meshkernel::volume(doc.meshSolid(pocket));
    const double expected = std::numbers::pi * 3.1 * 3.1 * 2.1;
    CHECK_THAT(removed, WithinRel(expected, 0.02));
    CHECK(history::describe(doc, pocket) == "Magnet pocket \xC3\x98" "6 \xC3\x97 2");
    // A stand-in magnet.
    CHECK(std::holds_alternative<CylinderSpec>(socket::matchingPart(magnet())));
}

TEST_CASE("Magnet: a buried pocket keeps a skin over it", "[magnet]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 0, "Head");
    const ObjectId pocket = addPocket(stack, doc, box, kTop, magnet(0.6));
    const MeshSolid s = doc.meshSolid(pocket);
    CHECK_THAT(8000.0 - meshkernel::volume(s), WithinRel(std::numbers::pi * 3.1 * 3.1 * 2.1, 0.02));
    // The top 0.6 mm over it is whole.
    Mesh slab;
    const float a = 4, z0 = 19.45f, z1 = 20;
    slab.positions = {-a, -a, z0, a, -a, z0, -a, a, z0, a, a, z0, -a, -a, z1, a, -a, z1, -a, a, z1, a, a, z1};
    slab.indices = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
    const MeshSolid top = meshkernel::fromTriangles(slab.positions, slab.indices);
    CHECK_THAT(meshkernel::volume(meshkernel::intersected({s, top})), WithinRel(meshkernel::volume(top), 1e-4));
    CHECK(socket::describe(magnet(0.6)) == "Magnet pocket \xC3\x98" "6 \xC3\x97 2, under 0.6 mm");
    SocketFinish thick = magnet(6.0);
    CHECK_THROWS_AS(socket::check(thick), std::invalid_argument);
}

TEST_CASE("Magnet: the matching pocket in the part against the face", "[magnet]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId neck = addBox(stack, doc, 0, "Neck");
    const ObjectId head = addBox(stack, doc, 20, "Head");
    const ObjectId apart = addBox(stack, doc, 60, "Far");
    const auto facing = facingSpot(doc, head, kTop);
    REQUIRE(facing);
    CHECK_THAT(facing->point[2], WithinAbs(20.0, 1e-6));
    CHECK_THAT(facing->normal[2], WithinAbs(-1.0, 1e-6));
    CHECK_FALSE(facingSpot(doc, apart, kTop));  // not against it

    const ObjectId p1 = addPocket(stack, doc, neck, kTop, magnet());
    const ObjectId p2 = addPocket(stack, doc, head, *facing, magnet());
    // The head's pocket is cut too.
    CHECK(meshkernel::volume(doc.meshSolid(p2)) < 8000.0 - 60.0);
    // Pose Test sees a fixed joint holding the head on.
    const auto joints = pose::findJoints(doc);
    REQUIRE(joints.size() == 2);
    CHECK(joints[0].kind == pose::JointKind::Fixed);
    CHECK(joints[0].name == "Magnet in Pocketed");
    CHECK(joints[0].other == p2);

    // Saved and opened again.
    const auto path = std::filesystem::temp_directory_path() / "forge3d-test-magnet.f3d";
    SocketFinish buried = magnet(0.4);
    const ObjectId p3 = addPocket(stack, doc, apart, {{0, 0, 80}, {0, 0, 1}}, buried);
    file::save(doc, path, "test");
    Document back;
    back.replaceAll(file::load(path));
    CHECK(*back.find(p3)->boolean->socket == buried);
    CHECK(*back.find(p1)->boolean->socket == magnet());
    std::filesystem::remove(path);
}
