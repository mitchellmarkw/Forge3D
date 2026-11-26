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
        CHECK(std::string(figurestyle::title(Style(s))).size() > 3);
        // Always a bigger head.
        CHECK(k(figurestyle::look(Style(s), made.points), rig::Proportion::Head) > 1.0);
    }
    const auto vinyl = figurestyle::look(Style::Vinyl, made.points);
    // Gentler at 100% than 0.113.0 made it: its 50%.
    CHECK_THAT(k(vinyl, rig::Proportion::Head), WithinAbs(1.64, 0.01));
    CHECK(k(vinyl, rig::Proportion::Legs) < 0.8);
    CHECK(k(vinyl, rig::Proportion::LimbThickness) > 1.2);
    CHECK(k(figurestyle::look(Style::Bobblehead, made.points), rig::Proportion::Head) > 2.0);
    // Stronger: further from as made; within the range.
    const auto strong = figurestyle::look(Style::Vinyl, made.points, 2.0);
    CHECK(k(strong, rig::Proportion::Head) > k(vinyl, rig::Proportion::Head));
    CHECK(k(strong, rig::Proportion::Head) <= rig::kMostProportion);
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
          k(figurestyle::look(Style::Caricature, made.points), rig::Proportion::Arms) + 0.1);
}

TEST_CASE("Figure styles: thicker limbs, a wider body", "[figurestyle][rig]")
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
    // A big head: the neck under it isn't pulled out into a wide lip.
    {
        PosedFinish f = made;
        f.proportions[std::size_t(rig::Proportion::Head)] = 2.4;
        const Mesh m = skin.posed(f).mesh;
        const double neckZ = made.points[std::size_t(Point::Neck)][2];
        double widest = 0;
        for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
            if (std::abs(m.positions[i + 2] - (neckZ - 3.0)) < 0.75 && std::abs(m.positions[i]) < 12.0)
                widest = std::max(widest, std::hypot(double(m.positions[i]), double(m.positions[i + 1])));
        CHECK(widest < 8.0);  // the neck's 5 mm thick; 0.113.0 made it about 10
    }
}

TEST_CASE("Figure styles: saved with the step", "[figurestyle]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    PosedFinish f = rig::fit(onPlate(doc, figure));
    f.proportions = figurestyle::look(figurestyle::Style::Vinyl, f.points).proportions;
    Body r;
    r.id = doc.allocateId();
    r.name = "Posed";
    r.boolean = posedSpec(doc, figure, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    CHECK(history::describe(doc, r.id) == "posed, as made, feet flat, new proportions");
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
    CHECK(doc.find(made.body)->name == "Body");
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

TEST_CASE("Figure styles: the neck's cut moved by hand", "[figurestyle]")
{
    const Mesh figure = trianglesOf(tFigure());
    const PosedFinish f = rig::fit(figure);
    const figurestyle::NeckCut found = figurestyle::neckCut(figure, f.points);
    CHECK(found.point[2] < f.points[std::size_t(Point::Neck)][2] + 0.01);
    CHECK(found.point[2] > 112.0);  // on the neck, above the shoulders
    CHECK_THAT(found.radius, WithinAbs(5.0, 1.0));
    const figurestyle::NeckCut lower = figurestyle::neckCut(figure, f.points, -3.0);
    CHECK_THAT(lower.point[2], WithinAbs(found.point[2] - 3.0, 0.05));
    const figurestyle::NeckCut tipped = figurestyle::neckCut(figure, f.points, 0.0, 20.0);
    CHECK_THAT(std::acos(tipped.normal[2]) * 180.0 / 3.14159265358979, WithinAbs(20.0, 1.0));
    // Out of range: kept in it.
    CHECK_THAT(figurestyle::neckCut(figure, f.points, 0.0, 90.0).tilt, WithinAbs(figurestyle::kMostTilt, 1e-9));
    const Mesh disc = figurestyle::cutDisc(found, 12.0);
    CHECK_THAT(double(disc.bounds().size()[0]), WithinAbs(24.0, 0.2));
    CHECK(meshkernel::volume(meshkernel::fromTriangles(disc.positions, disc.indices)) > 0.0);

    // Cut where it's moved to.
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = addFigure(stack, doc, figure);
    const PosedFinish onIt = rig::fit(onPlate(doc, id));
    const figurestyle::NeckCut there = figurestyle::neckCut(onPlate(doc, id), onIt.points, 2.0);
    const figurestyle::Bobble made = figurestyle::separateHead(stack, doc, id, onIt.points, {}, 2.0);
    CHECK_THAT(double(doc.worldBounds(made.head).min[2]), WithinAbs(there.point[2], 0.8));
}

TEST_CASE("Figure styles: a bobblehead's head marked with a mask", "[figurestyle]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const Bounds b = doc.worldBounds(figure);
    // A ball of mask over the head and the top of the neck.
    MaskDab dab;
    dab.centre = {b.center()[0], b.center()[1], b.max[2] - 12.0};
    dab.radius = 17.0;
    const double whole = meshkernel::volume(doc.meshSolid(figure));
    const std::size_t before = stack.count();
    const figurestyle::Bobble made = figurestyle::separateHeadByMask(stack, doc, figure, {dab}, {});
    CHECK(stack.count() == before + 1);
    CHECK(doc.find(made.head)->name == "Head");
    CHECK(doc.find(made.body)->name == "Body");
    const Bounds hb = doc.worldBounds(made.head), bb = doc.worldBounds(made.body);
    CHECK(hb.max[2] > b.max[2] - 0.5);   // the head is the top
    CHECK(hb.min[2] > b.max[2] - 32.0);  // and only the top
    CHECK(bb.max[2] > hb.min[2]);        // the post up into it
    const double head = meshkernel::volume(doc.meshSolid(made.head));
    const double body = meshkernel::volume(doc.meshSolid(made.body));
    CHECK(head < 0.3 * whole);
    CHECK_THAT(head + body, WithinRel(whole, 0.05));
    // Nothing masked: refused.
    CHECK_THROWS_AS(figurestyle::separateHeadByMask(stack, doc, made.body, {}, {}), std::invalid_argument);
}
TEST_CASE("Figure styles: the neck found under a big head", "[figurestyle]")
{
    // A big head (36 mm across) on a short neck: the skeleton's neck ball lands up in the
    // head, but the cut goes under the chin.
    MeshSolid big = tFigure();
    const MeshSolid head = meshkernel::levelSet(
        [](const Vec3& q) { return 18.0 - std::sqrt(q[0] * q[0] + (q[1] + 2.0) * (q[1] + 2.0) + (q[2] - 142.0) * (q[2] - 142.0)); },
        [] {
            Bounds b;
            b.expand(-20, -22, 122);
            b.expand(20, 18, 162);
            return b;
        }(),
        1.0, 0.0);
    big = meshkernel::merged({big, head});
    const Mesh figure = trianglesOf(big);
    const PosedFinish f = rig::fit(figure);
    const figurestyle::NeckCut nc = figurestyle::neckCut(figure, f.points);
    CHECK(nc.point[2] < 125.0);  // under the chin (the head's bottom is at 124)
    CHECK(nc.point[2] > 114.0);  // above the shoulders
    CHECK_THAT(nc.radius, WithinAbs(5.0, 1.0));
    // The middle of the neck, not of the skeleton's line.
    CHECK_THAT(nc.point[0], WithinAbs(0.0, 0.5));
    CHECK_THAT(nc.point[1], WithinAbs(0.0, 1.0));
}
