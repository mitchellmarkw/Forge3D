// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 65: Pose Figure (bones in one sculpted figure).
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/Mannequin.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Rig.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
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
MeshSolid tFigure()
{
    const std::vector<Capsule> parts{
        {{0, 0, 62}, {0, 0, 112}, 12},     // torso
        {{0, 0, 112}, {0, 0, 128}, 5},     // neck
        {{0, 0, 136}, {0, 0, 138}, 10},    // head
        {{12, 0, 118}, {72, 0, 118}, 4.5},  // left arm (+X)
        {{-12, 0, 118}, {-72, 0, 118}, 4.5},
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

double lowestZ(const Mesh& m)
{
    double lo = std::numeric_limits<double>::max();
    for (std::size_t i = 2; i < m.positions.size(); i += 3)
        lo = std::min(lo, double(m.positions[i]));
    return lo;
}

const Vec3& pt(const rig::Points& p, Point which) { return p[std::size_t(which)]; }

}  // namespace

TEST_CASE("Pose Figure: the skeleton is fitted to a T-posed figure", "[rig]")
{
    const Mesh figure = trianglesOf(tFigure());
    const PosedFinish f = rig::fit(figure);
    const auto& p = f.points;
    // Arms found out to the sides, the points in the middle of the arm.
    CHECK(pt(p, Point::LeftHand)[0] > 60);
    CHECK(pt(p, Point::RightHand)[0] < -60);
    CHECK(pt(p, Point::LeftElbow)[0] > 25);
    CHECK(pt(p, Point::LeftElbow)[0] < pt(p, Point::LeftWrist)[0]);
    CHECK_THAT(pt(p, Point::LeftElbow)[2], WithinAbs(118, 3));
    CHECK_THAT(pt(p, Point::LeftElbow)[1], WithinAbs(0, 2));
    // Legs: hips above knees above ankles, the toes in front.
    CHECK(pt(p, Point::LeftHip)[0] > 0);
    CHECK(pt(p, Point::RightHip)[0] < 0);
    CHECK(pt(p, Point::LeftHip)[2] > pt(p, Point::LeftKnee)[2]);
    CHECK(pt(p, Point::LeftKnee)[2] > pt(p, Point::LeftAnkle)[2]);
    // The hips at the crotch (the body's bottom, 50 mm up), the knees in the middle of the
    // leg (not pulled towards the other one).
    CHECK_THAT(pt(p, Point::LeftHip)[2], WithinAbs(50.0, 3.0));
    CHECK_THAT(pt(p, Point::LeftKnee)[0], WithinAbs(12.0 - 3.0 * (pt(p, Point::LeftKnee)[2] - 7.0) / 55.0, 1.5));
    CHECK(pt(p, Point::LeftToe)[1] < pt(p, Point::LeftAnkle)[1] - 5);
    CHECK(pt(p, Point::Head)[2] > 140);
    CHECK(f.blend > 0.5);

    // The angles it was made in: arms straight out, legs nearly straight down.
    const auto& a = f.angles;
    CHECK_THAT(a[std::size_t(Joint::LeftShoulder)][1], WithinAbs(90, 8));
    CHECK_THAT(a[std::size_t(Joint::RightShoulder)][1], WithinAbs(90, 8));
    CHECK_THAT(a[std::size_t(Joint::LeftElbow)][0], WithinAbs(0, 8));
    CHECK_THAT(a[std::size_t(Joint::LeftKnee)][0], WithinAbs(0, 10));
    CHECK_THAT(a[std::size_t(Joint::Waist)][0], WithinAbs(0, 3));
    CHECK(rig::describe(f) == "posed, as made, feet flat");
}

TEST_CASE("Pose Figure: as made it doesn't move; bent, it follows", "[rig]")
{
    const Mesh figure = trianglesOf(tFigure());
    PosedFinish f = rig::fit(figure);
    const rig::Skin skin(figure, f);

    SECTION("at the angles it was made in")
    {
        const auto posed = skin.posed(f);
        double worst = 0;
        for (std::size_t i = 0; i < figure.positions.size(); ++i)
            worst = std::max(worst, double(std::abs(posed.mesh.positions[i] - figure.positions[i])));
        CHECK(worst < 1e-3);
        for (int i = 0; i < kRigPointCount; ++i)
            for (std::size_t k = 0; k < 3; ++k)
                CHECK_THAT(posed.points[std::size_t(i)][k], WithinAbs(f.points[std::size_t(i)][k], 1e-6));
    }
    SECTION("the left elbow bent forward")
    {
        f.angles[std::size_t(Joint::LeftElbow)][0] = 90;
        const auto posed = skin.posed(f);
        const Vec3 elbow = pt(posed.points, Point::LeftElbow);
        const Vec3 hand = pt(posed.points, Point::LeftHand);
        CHECK_THAT(hand[0], WithinAbs(elbow[0], 4));
        CHECK(hand[1] < elbow[1] - 20);  // forward is -Y
        CHECK_THAT(hand[2], WithinAbs(elbow[2], 4));
        // The right arm and the legs stay put.
        double rightMoved = 0;
        for (std::size_t i = 0; i < figure.vertexCount(); ++i)
            if (figure.positions[3 * i] < -30)
                for (std::size_t k = 0; k < 3; ++k)
                    rightMoved = std::max(rightMoved,
                                          double(std::abs(posed.mesh.positions[3 * i + k] - figure.positions[3 * i + k])));
        CHECK(rightMoved < 1e-3);
        // Bent, the arm keeps its thickness (the volume hardly changes).
        const double before = meshkernel::volume(meshkernel::fromTriangles(figure.positions, figure.indices));
        const double after =
            meshkernel::volume(meshkernel::fromTriangles(posed.mesh.positions, posed.mesh.indices));
        CHECK_THAT(after, WithinRel(before, 0.03));
    }
    SECTION("a figure guide pose: standing, arms down")
    {
        f.angles = rig::fromMannequin(mannequin::preset(mannequin::PosePreset::Standing));
        const auto posed = skin.posed(f);
        const Vec3 shoulder = pt(posed.points, Point::LeftShoulder);
        const Vec3 hand = pt(posed.points, Point::LeftHand);
        CHECK(hand[2] < shoulder[2] - 40);
        CHECK(std::abs(hand[0] - shoulder[0]) < 15);
        CHECK(hand[0] > 0);  // still the left side
    }
    SECTION("knees bent: it stays on the ground, feet flat")
    {
        f.angles[std::size_t(Joint::LeftHip)][0] = 45;
        f.angles[std::size_t(Joint::RightHip)][0] = 45;
        f.angles[std::size_t(Joint::LeftKnee)][0] = 90;
        f.angles[std::size_t(Joint::RightKnee)][0] = 90;
        const auto posed = skin.posed(f);
        CHECK_THAT(lowestZ(posed.mesh), WithinAbs(lowestZ(figure), 1e-3));
        CHECK(pt(posed.points, Point::Head)[2] < pt(f.points, Point::Head)[2] - 10);
        // Flat feet: the toes are as far below the ankle as they were.
        const double drop = pt(f.points, Point::LeftAnkle)[2] - pt(f.points, Point::LeftToe)[2];
        CHECK_THAT(pt(posed.points, Point::LeftAnkle)[2] - pt(posed.points, Point::LeftToe)[2],
                   WithinAbs(drop, 1e-6));
        // Not flat: the feet follow the shins.
        f.feetFlat = false;
        const auto tipped = skin.posed(f);
        CHECK(std::abs(pt(tipped.points, Point::LeftAnkle)[2] - pt(tipped.points, Point::LeftToe)[2] - drop) > 2);
    }
}

TEST_CASE("Pose Figure: settings are checked", "[rig]")
{
    const PosedFinish good = rig::fit(trianglesOf(tFigure()));
    PosedFinish f = good;
    f.blend = 0;
    CHECK_THROWS_AS(rig::check(f), std::invalid_argument);
    f = good;
    f.points[std::size_t(Point::LeftElbow)] = f.points[std::size_t(Point::LeftShoulder)];
    CHECK_THROWS_AS(rig::check(f), std::invalid_argument);
    f = good;
    f.angles[0][0] = std::nan("");
    CHECK_THROWS_AS(rig::check(f), std::invalid_argument);
    CHECK(rig::mirrored(Point::LeftKnee) == Point::RightKnee);
    CHECK(rig::mirrored(Point::RightToe) == Point::LeftToe);
    CHECK(rig::mirrored(Point::Neck) == Point::Neck);
    CHECK(rig::pivot(Joint::LeftElbow) == Point::LeftElbow);
    CHECK(rig::skeletonMesh(good.points, 2.0, 3).triangleCount() > 0);
}

TEST_CASE("Pose Figure: a step in the history, saved and rescaled", "[rig]")
{
    Document doc;
    CommandStack stack(doc);
    const Mesh figure = trianglesOf(tFigure());
    auto data = std::make_shared<MeshData>();
    data->positions = figure.positions;
    data->indices = figure.indices;
    const Bounds b = figure.bounds();
    // Centred on X and Y with the lowest at Z = 0, like every primitive.
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

    // Fitted in the step's coordinates (the plate's, for a figure on it).
    const auto mesh = doc.mesh(body.id);
    Mesh placed = *mesh;
    const Transform t = doc.localTransform(body.id);
    for (std::size_t i = 0; i < placed.positions.size(); i += 3) {
        const Vec3 p = t.apply(Vec3{placed.positions[i], placed.positions[i + 1], placed.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            placed.positions[i + k] = float(p[k]);
    }
    PosedFinish f = rig::fit(placed);
    f.angles[std::size_t(Joint::LeftElbow)][0] = 90;
    f.angles[std::size_t(Joint::RightShoulder)][1] = 20;

    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Posed");
    r.boolean = posedSpec(doc, body.id, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    CHECK(history::describe(doc, r.id) == "posed, 2 joints moved, feet flat");
    const double before = meshkernel::volume(doc.meshSolid(body.id));
    CHECK_THAT(meshkernel::volume(doc.meshSolid(r.id)), WithinRel(before, 0.04));
    CHECK(doc.worldBounds(r.id).size()[1] > doc.worldBounds(body.id).size()[1] + 15);  // the forearm points forward

    // Changed, then undone.
    PosedFinish straight = f;
    straight.angles[std::size_t(Joint::LeftElbow)] = rig::restAngles(f.points)[std::size_t(Joint::LeftElbow)];
    const Body changed = withPosed(doc, r.id, straight);
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(r.id), changed, "Pose Figure"));
    CHECK(history::describe(doc, r.id) == "posed, 1 joint moved, feet flat");
    stack.undo();
    CHECK(doc.find(r.id)->boolean->posed->angles[std::size_t(Joint::LeftElbow)][0] == 90);

    const auto path = std::filesystem::temp_directory_path() / "forge_rig_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    const auto found = std::find_if(back.begin(), back.end(), [&](const Body& x) { return x.id == r.id; });
    REQUIRE(found != back.end());
    REQUIRE(found->boolean->posed);
    CHECK(*found->boolean->posed == *doc.find(r.id)->boolean->posed);

    // Rescaled: the skeleton grows with the figure.
    const Vec3 head = f.points[std::size_t(Point::Head)];
    auto after = rescaledDesign(doc, {r.id}, 2.0, {0, 0, 0}, ScaleOptions{});
    std::vector<Body> old;
    for (const Body& x : after)
        old.push_back(*doc.find(x.id));
    stack.execute(std::make_unique<EditBodiesCommand>(old, after, "Rescale"));
    const PosedFinish& grown = *doc.find(r.id)->boolean->posed;
    CHECK_THAT(grown.points[std::size_t(Point::Head)][2], WithinAbs(2 * head[2], 1e-6));
    CHECK_THAT(grown.blend, WithinAbs(2 * f.blend, 1e-9));
}
