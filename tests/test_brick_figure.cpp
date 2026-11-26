// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 107: brick figure.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/BrickFigure.h"
#include "forge/core/LimbCut.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Rig.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using rig::Joint;
using rig::Point;

namespace {

struct Capsule {
    Vec3 a, b;
    double r;
};

double capsuleField(const Capsule& c, const Vec3& q)
{
    const Vec3 d{c.b[0] - c.a[0], c.b[1] - c.a[1], c.b[2] - c.a[2]};
    const Vec3 e{q[0] - c.a[0], q[1] - c.a[1], q[2] - c.a[2]};
    const double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    const double t = std::clamp((e[0] * d[0] + e[1] * d[1] + e[2] * d[2]) / dd, 0.0, 1.0);
    const Vec3 f{e[0] - t * d[0], e[1] - t * d[1], e[2] - t * d[2]};
    return c.r - std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
}

// A figure about 148 mm tall in a T pose, made of rounded rods, standing on the plate
// and facing -Y.
MeshSolid tFigure(double armDrop = 0.0)
{
    // The arms 60 mm long from the shoulders, dropped this many degrees from level.
    const double c = 60.0 * std::cos(armDrop * 3.14159265358979 / 180.0);
    const double d = 60.0 * std::sin(armDrop * 3.14159265358979 / 180.0);
    const std::vector<Capsule> parts{
        {{0, 0, 62}, {0, 0, 112}, 12},     // torso
        {{0, 0, 112}, {0, 0, 128}, 5},     // neck
        {{0, 0, 136}, {0, 0, 138}, 10},    // head
        {{12, 0, 118}, {12 + c, 0, 118 - d}, 4.5},  // left arm (+X)
        {{-12, 0, 118}, {-12 - c, 0, 118 - d}, 4.5},
        {{9, 0, 62}, {12, 0, 7}, 6},       // left leg
        {{-9, 0, 62}, {-12, 0, 7}, 6},
        {{12, 2, 4}, {12, -14, 3.5}, 3.5},  // feet
        {{-12, 2, 4}, {-12, -14, 3.5}, 3.5},
    };
    const auto field = [&](const Vec3& q) {
        double best = -1e9;
        for (const Capsule& c : parts)
            best = std::max(best, capsuleField(c, q));
        return best;
    };
    Bounds box;
    box.expand(-85, -25, -2);
    box.expand(85, 20, 152);
    return meshkernel::levelSet(field, box, 1.5, 0.0);
}

Mesh trianglesOf(const MeshSolid& s)
{
    Mesh m;
    meshkernel::triangles(s, m.positions, m.indices);
    return m;
}

ObjectId addFigure(CommandStack& stack, Document& doc, const Mesh& figure)
{
    auto data = std::make_shared<MeshData>();
    data->positions = figure.positions;
    data->indices = figure.indices;
    const Bounds b = figure.bounds();
    const float cx = b.center()[0], cy = b.center()[1], z0 = b.min[2];
    for (std::size_t i = 0; i < data->positions.size(); i += 3) {
        data->positions[i] -= cx;
        data->positions[i + 1] -= cy;
        data->positions[i + 2] -= z0;
    }
    data->size = b.size();
    MeshSpec spec;
    spec.data = data;
    spec.width = b.size()[0];
    spec.depth = b.size()[1];
    spec.height = b.size()[2];
    Body body;
    body.id = doc.allocateId();
    body.spec = spec;
    body.name = "Figure";
    stack.execute(std::make_unique<AddBodyCommand>(body));
    return body.id;
}

Mesh onPlate(const Document& doc, ObjectId id)
{
    Mesh m = *doc.mesh(id);
    const Transform t = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        const Vec3 p = t.apply(Vec3{m.positions[i], m.positions[i + 1], m.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            m.positions[i + k] = float(p[k]);
    }
    return m;
}

}  // namespace

TEST_CASE("Brick figure: the body's parts", "[brickfig]")
{
    brickfig::Settings s;
    s.height = 60.0;
    const std::vector<brickfig::Part> parts = brickfig::body(s);
    REQUIRE(parts.size() == 5);
    std::vector<std::string> names;
    double lowest = 1e9, highest = -1e9;
    std::vector<MeshSolid> solids;
    for (const auto& p : parts) {
        names.push_back(p.name);
        const MeshSolid m = meshkernel::fromTriangles(p.mesh.positions, p.mesh.indices);
        CHECK(meshkernel::volume(m) > 50.0);
        const Bounds b = p.mesh.bounds();
        lowest = std::min(lowest, double(b.min[2]));
        highest = std::max(highest, double(b.max[2]));
        solids.push_back(m);
    }
    CHECK(names == std::vector<std::string>{"Torso", "Left arm", "Right arm", "Left leg", "Right leg"});
    CHECK_THAT(lowest, WithinAbs(0.0, 0.5));  // standing on the plate
    const brickfig::Seat seat = brickfig::seatFor(s);
    CHECK_THAT(highest, WithinAbs(seat.z + seat.postHeight, 0.5));
    CHECK_THAT(seat.z, WithinAbs(0.70 * s.height, 1e-9));
    // The parts don't overlap (they're made the fit apart, pegs in holes).
    for (std::size_t i = 0; i < solids.size(); ++i)
        for (std::size_t j = i + 1; j < solids.size(); ++j)
            CHECK(meshkernel::volume(meshkernel::intersected({solids[i], solids[j]})) < 0.5);
    // The left parts on the left (+X).
    CHECK(parts[1].mesh.bounds().min[0] > 0.0);
    CHECK(parts[3].mesh.bounds().min[0] > 0.0);
    CHECK(parts[2].mesh.bounds().max[0] < 0.0);
    // One piece.
    s.build = brickfig::Build::OnePiece;
    const auto one = brickfig::body(s);
    REQUIRE(one.size() == 1);
    CHECK(one[0].name == "Body");
    // Checked.
    s.height = 10.0;
    CHECK_THROWS_AS(brickfig::check(s), std::invalid_argument);
    s.height = 30.0;
    s.grip = 8.0;
    CHECK_THROWS_AS(brickfig::check(s), std::invalid_argument);
}

TEST_CASE("Brick figure: the figure's head on a brick body", "[brickfig]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    const std::size_t before = stack.count();
    brickfig::Settings s;
    s.height = 80.0;
    const brickfig::Made made = brickfig::make(stack, doc, figure, skeleton.points, s);
    CHECK(stack.count() == before + 1);
    REQUIRE(made.parts.size() == 6);
    CHECK(doc.find(made.parts[0])->name == "Brick head");
    CHECK(doc.isTopLevel(figure));  // the figure stays
    const Bounds head = doc.worldBounds(made.parts[0]);
    const Bounds torso = doc.worldBounds(made.parts[1]);
    // The head the top 30% of the height, on the torso's top, its socket over the post.
    CHECK_THAT(double(head.size()[2]), WithinAbs(0.30 * s.height, 1.0));
    CHECK_THAT(double(head.min[2]), WithinAbs(0.70 * s.height, 0.8));
    CHECK(torso.max[2] > head.min[2]);  // the post up into it
    const auto onPlateSolid = [&](ObjectId id) { return meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id)); };
    CHECK(meshkernel::volume(meshkernel::intersected({onPlateSolid(made.parts[0]), onPlateSolid(made.parts[1])})) < 0.5);
    // Beside the figure.
    CHECK(head.min[0] > doc.worldBounds(figure).max[0]);
    stack.undo();
    CHECK(doc.find(made.parts[0]) == nullptr);

    // One piece: one shape.
    s.build = brickfig::Build::OnePiece;
    const brickfig::Made one = brickfig::make(stack, doc, figure, skeleton.points, s);
    REQUIRE(one.parts.size() == 1);
    CHECK(doc.find(one.parts[0])->name == "Brick figure");
    CHECK_THAT(double(doc.worldBounds(one.parts[0]).size()[2]), WithinAbs(s.height, 1.5));
}
TEST_CASE("Brick figure: a big head cut under its chin, centred on the post", "[brickfig]")
{
    Document doc;
    CommandStack stack(doc);
    MeshSolid big = tFigure();
    Bounds hb;
    hb.expand(-20, -24, 122);
    hb.expand(20, 18, 164);
    // A big head, set a little forward of the neck.
    big = meshkernel::merged({big, meshkernel::levelSet(
                                       [](const Vec3& q) {
                                           return 18.0 - std::sqrt(q[0] * q[0] + (q[1] + 3.0) * (q[1] + 3.0) +
                                                                   (q[2] - 142.0) * (q[2] - 142.0));
                                       },
                                       hb, 1.0, 0.0)});
    const ObjectId figure = addFigure(stack, doc, trianglesOf(big));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    brickfig::Settings s;
    s.height = 60.0;
    const brickfig::Made made = brickfig::make(stack, doc, figure, skeleton.points, s);
    const Bounds head = doc.worldBounds(made.parts[0]);
    const Bounds torso = doc.worldBounds(made.parts[1]);
    // All the head (36 mm round, cut under it): 30% of the height, a sphere's width.
    CHECK_THAT(double(head.size()[2]), WithinAbs(0.30 * s.height, 1.0));
    CHECK(head.size()[0] > 0.8 * head.size()[2]);
    // Its neck on the post's middle: the head a little forward of the torso's middle (as it
    // was of its neck), not more.
    CHECK_THAT(double(head.center()[0]), WithinAbs(double(torso.center()[0]), 0.5));
    CHECK(head.center()[1] < torso.center()[1] + 0.5);
    CHECK(head.center()[1] > torso.center()[1] - 3.0);
    // Moved back by hand.
    stack.undo();
    s.headForward = -2.0;
    const brickfig::Made back = brickfig::make(stack, doc, figure, skeleton.points, s);
    CHECK_THAT(double(doc.worldBounds(back.parts[0]).center()[1]), WithinAbs(double(head.center()[1]) + 2.0, 0.3));
}
