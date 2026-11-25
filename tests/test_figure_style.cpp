// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 105: figure styles.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/FigureStyle.h"
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

TEST_CASE("Figure styles: each style's proportions", "[figurestyle]")
{
    const PosedFinish made = rig::fit(trianglesOf(tFigure()));
    using figurestyle::Style;
    const auto k = [](const figurestyle::Look& l, rig::Proportion p) { return l.proportions[std::size_t(p)]; };
    for (int s = 0; s < figurestyle::kStyleCount; ++s) {
        // No strength: as made.
        const auto none = figurestyle::look(Style(s), made.points, 0.0);
        CHECK(none.proportions == rig::asMade());
        CHECK(none.blocky == 0.0);
        CHECK(std::string(figurestyle::title(Style(s))).size() > 3);
        // Always a bigger head.
        CHECK(k(figurestyle::look(Style(s), made.points), rig::Proportion::Head) > 1.0);
    }
    const auto vinyl = figurestyle::look(Style::Vinyl, made.points);
    CHECK(k(vinyl, rig::Proportion::Head) > 2.0);
    CHECK(k(vinyl, rig::Proportion::Legs) < 0.7);
    CHECK(k(vinyl, rig::Proportion::LimbThickness) > 1.3);
    // Stronger: further from as made; within the range.
    const auto strong = figurestyle::look(Style::Vinyl, made.points, 1.5);
    CHECK(k(strong, rig::Proportion::Head) > k(vinyl, rig::Proportion::Head));
    CHECK(k(strong, rig::Proportion::Head) <= rig::kMostProportion);
    CHECK(figurestyle::look(Style::Blocky, made.points).blocky == 1.0);
    CHECK(figurestyle::look(Style::Blocky, made.points, 0.5).blocky == 0.5);
    // A caricature makes long arms longer still.
    PosedFinish longArms = made;
    for (Point p : {Point::LeftElbow, Point::LeftWrist, Point::LeftHand}) {
        auto& q = longArms.points[std::size_t(p)];
        const auto& s = longArms.points[std::size_t(Point::LeftShoulder)];
        q[0] = s[0] + 1.6 * (q[0] - s[0]);
    }
    for (Point p : {Point::RightElbow, Point::RightWrist, Point::RightHand}) {
        auto& q = longArms.points[std::size_t(p)];
        const auto& s = longArms.points[std::size_t(Point::RightShoulder)];
        q[0] = s[0] + 1.6 * (q[0] - s[0]);
    }
    CHECK(k(figurestyle::look(Style::Caricature, longArms.points), rig::Proportion::Arms) >
          k(figurestyle::look(Style::Caricature, made.points), rig::Proportion::Arms) + 0.2);
}

TEST_CASE("Figure styles: thicker limbs, a wider body, squared off", "[figurestyle][rig]")
{
    const Mesh figure = trianglesOf(tFigure());
    const PosedFinish made = rig::fit(figure);
    const rig::Skin skin(figure, made);
    const auto volumeOf = [](const Mesh& m) { return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices)); };
    const Mesh before = skin.posed(made).mesh;
    const double v0 = volumeOf(before);
    {
        PosedFinish f = made;
        f.proportions[std::size_t(rig::Proportion::LimbThickness)] = 1.5;
        const Mesh m = skin.posed(f).mesh;
        CHECK(volumeOf(m) > 1.15 * v0);
        // As long as before: the arms' reach the same.
        CHECK_THAT(double(m.bounds().size()[0]), WithinAbs(double(before.bounds().size()[0]), 6.0));
    }
    {
        PosedFinish f = made;
        f.proportions[std::size_t(rig::Proportion::BodyWidth)] = 1.3;
        const Mesh m = skin.posed(f).mesh;
        CHECK(m.bounds().size()[1] > before.bounds().size()[1] + 2.5);  // the torso deeper
        CHECK(volumeOf(m) > 1.1 * v0);
    }
    {
        PosedFinish f = made;
        f.blocky = 1.0;
        CHECK(rig::shaped(f));
        CHECK_FALSE(rig::proportioned(f));
        const Mesh m = skin.posed(f).mesh;
        const double v = volumeOf(m);
        CHECK(v > 1.1 * v0);  // a square round each circle
        CHECK(v < 1.4 * v0);
        // The torso's surface at 45 degrees round it is pushed out, square to it isn't.
        double diagonal = 0, front = 0;
        for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
            const double x0 = before.positions[i], y0 = before.positions[i + 1], z0 = before.positions[i + 2];
            if (z0 < 80 || z0 > 100)
                continue;
            const double r0 = std::hypot(x0, y0);
            const double r = std::hypot(double(m.positions[i]), double(m.positions[i + 1]));
            const double angle = std::abs(std::atan2(std::abs(y0), std::abs(x0)) * 180.0 / 3.14159265358979 - 45.0);
            if (angle < 5)
                diagonal = std::max(diagonal, r - r0);
            else if (angle > 40)
                front = std::max(front, std::abs(r - r0));
        }
        CHECK(diagonal > 3.0);
        CHECK(front < 1.0);
        f.blocky = 1.5;
        CHECK_THROWS_AS(rig::check(f), std::invalid_argument);
    }
}

TEST_CASE("Figure styles: saved with the step", "[figurestyle]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    PosedFinish f = rig::fit(onPlate(doc, figure));
    const auto look = figurestyle::look(figurestyle::Style::Blocky, f.points);
    f.proportions = look.proportions;
    f.blocky = look.blocky;
    Body r;
    r.id = doc.allocateId();
    r.name = "Posed";
    r.boolean = posedSpec(doc, figure, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    CHECK(history::describe(doc, r.id) == "posed, as made, feet flat, new proportions, blocky");
    const auto path = std::filesystem::temp_directory_path() / "forge_figure_style_test.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(*other.find(r.id)->boolean->posed == *doc.find(r.id)->boolean->posed);
    CHECK_THAT(meshkernel::volume(other.meshSolid(r.id)), WithinRel(meshkernel::volume(doc.meshSolid(r.id)), 1e-6));
}

TEST_CASE("Figure styles: a bobblehead's head cut off for a spring", "[figurestyle][limbcut]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    const double whole = meshkernel::volume(doc.meshSolid(figure));
    const std::size_t undos = stack.count();

    // Too wide for the neck: nothing changes.
    figurestyle::Spring wide;
    wide.diameter = 14.0;
    CHECK_THROWS_AS(figurestyle::separateHead(stack, doc, figure, skeleton.points, wide), std::invalid_argument);
    CHECK(stack.count() == undos);
    CHECK(doc.isTopLevel(figure));

    const figurestyle::Spring spring;  // 5 mm
    const auto sizes = figurestyle::sizesFor(spring);
    CHECK_THAT(sizes.post, WithinAbs(3.9, 1e-9));
    CHECK_THAT(sizes.socket, WithinAbs(5.4, 1e-9));
    const figurestyle::Bobble made = figurestyle::separateHead(stack, doc, figure, skeleton.points, spring);
    CHECK(stack.count() == undos + 1);  // one step
    REQUIRE(doc.isTopLevel(made.head));
    REQUIRE(doc.isTopLevel(made.body));
    CHECK(doc.find(made.head)->name == "Head");
    CHECK(doc.find(made.body)->name == "Torso");
    const double head = meshkernel::volume(doc.meshSolid(made.head));
    const double body = meshkernel::volume(doc.meshSolid(made.body));
    // The socket out of the head, the post on the body: about their sizes.
    const double socket = 3.14159 * 2.7 * 2.7 * sizes.socketDepth;
    const double post = 3.14159 * 1.95 * 1.95 * sizes.postHeight;
    CHECK_THAT(head + body, WithinAbs(whole - socket + post, 0.35 * socket));
    // The head is above the post's top, the post reaching up into the socket.
    const Bounds hb = doc.worldBounds(made.head), bb = doc.worldBounds(made.body);
    CHECK(bb.max[2] > hb.min[2]);  // the post goes up into the head's socket
    CHECK(hb.max[2] > bb.max[2] + 10.0);
    stack.undo();
    CHECK(doc.isTopLevel(figure));
}


TEST_CASE("Figure styles: a bobblehead styled, then its head cut off", "[figurestyle][limbcut]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const Mesh plate = onPlate(doc, figure);
    PosedFinish f = rig::fit(plate);
    f.proportions = figurestyle::look(figurestyle::Style::Bobblehead, f.points).proportions;
    Body r;
    r.id = doc.allocateId();
    r.name = "Bobblehead";
    r.boolean = posedSpec(doc, figure, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    const rig::Points points = rig::Skin(plate, f).posed(f).points;
    const double whole = meshkernel::volume(doc.meshSolid(r.id));
    const figurestyle::Bobble made = figurestyle::separateHead(stack, doc, r.id, points, {});
    const double head = meshkernel::volume(doc.meshSolid(made.head));
    const double body = meshkernel::volume(doc.meshSolid(made.body));
    CHECK(head > 0.3 * whole);  // the big head
    CHECK(body > 0.3 * whole);
    // Cut at the neck's narrowest, under the neck's ball: the head's piece reaches below it.
    CHECK(doc.worldBounds(made.head).min[2] < points[std::size_t(Point::Neck)][2]);
    CHECK(doc.worldBounds(made.head).min[2] > points[std::size_t(Point::LeftShoulder)][2]);
}
