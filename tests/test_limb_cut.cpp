// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 68: Cut into Parts.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/LimbCut.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Rig.h"

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

TEST_CASE("Cut into Parts: a cut square to the limb", "[limbcut]")
{
    const MeshSolid solid = tFigure();
    const Mesh figure = trianglesOf(solid);
    const PosedFinish skeleton = rig::fit(figure);
    const limbcut::Cut shoulder = limbcut::plan(figure, skeleton.points, Joint::LeftShoulder);
    CHECK(shoulder.finish.normal[0] > 0.95);  // along the arm, out to the left
    CHECK_THAT(shoulder.limbRadius, WithinAbs(4.5, 1.2));
    CHECK(shoulder.finish.reach.size() == 3);  // upper arm, forearm, hand
    const limbcut::Cut knee = limbcut::plan(figure, skeleton.points, Joint::LeftKnee);
    CHECK(knee.finish.normal[2] < -0.9);  // down the leg
    for (const LimbRod& r : knee.finish.reach)
        CHECK(r.radius < 11.0);  // about as fat as the leg (6 mm): not reaching the other one, 20 mm away

    LimbCutFinish limb = shoulder.finish, rest = shoulder.finish;
    limb.limb = true;
    rest.limb = false;
    const double whole = meshkernel::volume(solid);
    const double arm = meshkernel::volume(limbcut::apply(solid, limb));
    const double body = meshkernel::volume(limbcut::apply(solid, rest));
    CHECK_THAT(arm + body, WithinRel(whole, 0.01));
    CHECK(arm > 2500.0);  // the arm, about 60 mm of 4.5 mm rod
    CHECK(arm < 5000.0);
    CHECK(limbcut::describe(limb) == "the left arm, cut at the left shoulder");
    CHECK(limbcut::describe(rest) == "the rest, cut at the left shoulder");

    // Opened up: each side cut back half the gap.
    limb.gap = 2.0;
    const double shorter = meshkernel::volume(limbcut::apply(solid, limb));
    CHECK_THAT(arm - shorter, WithinRel(3.14159 * 4.5 * 4.5, 0.25));

    LimbCutFinish bad = limb;
    bad.reach.clear();
    CHECK_THROWS_AS(limbcut::check(bad), std::invalid_argument);
}

TEST_CASE("Cut into Parts: arms angled down are still cut off square to the body", "[limbcut]")
{
    // An A pose: the arms 45 degrees down. The shoulder cut is upright, not square to the arm,
    // so it takes the arm and none of the body beside it.
    const MeshSolid solid = tFigure(45.0);
    const Mesh figure = trianglesOf(solid);
    const PosedFinish skeleton = rig::fit(figure);
    const limbcut::Cut left = limbcut::plan(figure, skeleton.points, Joint::LeftShoulder);
    const limbcut::Cut right = limbcut::plan(figure, skeleton.points, Joint::RightShoulder);
    CHECK_THAT(left.finish.normal[0], WithinAbs(1.0, 1e-9));
    CHECK_THAT(right.finish.normal[0], WithinAbs(-1.0, 1e-9));
    CHECK_THAT(limbcut::plan(figure, skeleton.points, Joint::LeftHip).finish.normal[2], WithinAbs(-1.0, 1e-9));
    const limbcut::Cut neck = limbcut::plan(figure, skeleton.points, Joint::Neck);
    CHECK_THAT(neck.finish.normal[2], WithinAbs(1.0, 1e-9));
    CHECK(neck.limbRadius < 8.0);  // the neck (5 mm), not the head (10 mm)
    CHECK_THAT(left.limbRadius, WithinAbs(4.5, 1.2));  // the arm's thickness, though cut at a slant
    // The shoulder's ball found where the arm meets the body, on the arm's line.
    const Vec3 shoulder = skeleton.points[std::size_t(Point::LeftShoulder)];
    CHECK(shoulder[0] > 12.0);
    CHECK(shoulder[0] < 22.0);
    CHECK_THAT(shoulder[2], WithinAbs(118.0 - (shoulder[0] - 12.0), 2.0));

    // Turned by hand: tipped 30 degrees towards the front, then 20 round the front-back line.
    const limbcut::Cut forward = limbcut::plan(figure, skeleton.points, Joint::LeftShoulder, {30.0, 0.0});
    CHECK_THAT(forward.finish.normal[0], WithinAbs(std::cos(30.0 * 3.14159265358979 / 180.0), 1e-9));
    CHECK_THAT(forward.finish.normal[1], WithinAbs(-0.5, 1e-9));  // towards the front (-Y)
    // Tilted sideways: square to an arm hanging down, both sides alike for the same numbers.
    const limbcut::Cut down = limbcut::plan(figure, skeleton.points, Joint::LeftShoulder, {0.0, 20.0});
    CHECK_THAT(down.finish.normal[2], WithinAbs(-std::sin(20.0 * 3.14159265358979 / 180.0), 1e-9));
    CHECK_THAT(down.finish.normal[1], WithinAbs(0.0, 1e-9));
    const limbcut::Cut rightDown = limbcut::plan(figure, skeleton.points, Joint::RightShoulder, {0.0, 20.0});
    CHECK_THAT(rightDown.finish.normal[2], WithinAbs(down.finish.normal[2], 1e-9));
    CHECK_THAT(rightDown.finish.normal[0], WithinAbs(-down.finish.normal[0], 1e-9));
    const Vec3 level = limbcut::tilted({0, 0, -1}, {});
    CHECK(level == Vec3{0, 0, -1});

    LimbCutFinish limb = left.finish;
    limb.limb = true;
    const double arm = meshkernel::volume(limbcut::apply(solid, limb));
    const double wholeArm = 3.14159 * 4.5 * 4.5 * 60.0 + 4.0 / 3.0 * 3.14159 * 4.5 * 4.5 * 4.5;
    CHECK(arm > 0.5 * wholeArm);
    CHECK(arm < wholeArm);  // nothing of the torso with it
    // The body keeps its full width at the shoulders.
    LimbCutFinish rest = left.finish;
    rest.limb = false;
    const Bounds body = meshkernel::bounds(limbcut::apply(solid, rest));
    CHECK(body.max[0] > left.finish.point[0] - 0.5);
}

TEST_CASE("Cut into Parts: a figure cut and jointed in one go", "[limbcut]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    const double whole = meshkernel::volume(doc.meshSolid(figure));

    std::vector<limbcut::Choice> choices;
    const auto add = [&](Joint j, limbcut::JointKind kind) {
        limbcut::Choice c;
        c.joint = j;
        c.kind = kind;
        c.ball = limbcut::ballFor(4.5);
        c.hinge = limbcut::hingeFor(4.5, HingeKind::Single);
        choices.push_back(c);
    };
    add(Joint::LeftElbow, limbcut::JointKind::Hinge);  // given out of order: cut from the middle out
    add(Joint::Neck, limbcut::JointKind::Ball);
    add(Joint::LeftShoulder, limbcut::JointKind::Ball);
    add(Joint::RightShoulder, limbcut::JointKind::Cut);
    add(Joint::LeftHip, limbcut::JointKind::Cut);
    const limbcut::Made made = limbcut::cutIntoParts(stack, doc, figure, skeleton.points, choices, 100);

    std::set<std::string> names;
    for (ObjectId id : made.pieces) {
        REQUIRE(doc.isTopLevel(id));
        names.insert(doc.find(id)->name);
        CHECK(meshkernel::volume(doc.meshSolid(id)) > 50.0);
    }
    CHECK(names == std::set<std::string>{"Torso", "Head", "Left upper arm", "Left forearm", "Right arm", "Left leg"});
    CHECK(made.extras.size() == 1);  // the elbow's pin
    CHECK(stack.undoLabel() == "Cut into Parts");
    // Nothing lost but what the joints take away and the gaps.
    double sum = 0.0;
    for (ObjectId id : made.pieces)
        sum += meshkernel::volume(doc.meshSolid(id));
    CHECK(sum < whole * 1.02);
    CHECK(sum > whole * 0.9);

    // Saved and opened again.
    const auto path = std::filesystem::temp_directory_path() / "forge_limbcut_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    std::size_t cuts = 0;
    for (const Body& b : back)
        if (b.boolean && b.boolean->op == BooleanOp::LimbCut) {
            ++cuts;
            const Body* now = doc.find(b.id);
            REQUIRE(now);
            CHECK(*b.boolean->limbCut == *now->boolean->limbCut);
        }
    CHECK(cuts >= 10);  // two per cut (and the copies inside the pieces cut again)

    stack.undo();
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{figure});
}

TEST_CASE("Cut into Parts: the usual figure (timing)", "[.bench][limbcut]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    std::vector<limbcut::Choice> choices;
    for (Joint j : {Joint::Neck, Joint::LeftShoulder, Joint::RightShoulder, Joint::LeftHip, Joint::RightHip,
                    Joint::LeftElbow, Joint::RightElbow, Joint::LeftKnee, Joint::RightKnee}) {
        limbcut::Choice c;
        c.joint = j;
        const bool hinge = j == Joint::LeftElbow || j == Joint::RightElbow || j == Joint::LeftKnee || j == Joint::RightKnee;
        c.kind = hinge ? limbcut::JointKind::Hinge : limbcut::JointKind::Ball;
        c.ball = limbcut::ballFor(4.5);
        c.hinge = limbcut::hingeFor(4.5, HingeKind::Single);
        choices.push_back(c);
    }
    const auto start = std::chrono::steady_clock::now();
    const limbcut::Made made = limbcut::cutIntoParts(stack, doc, figure, skeleton.points, choices, 1);
    for (ObjectId id : made.pieces)
        doc.meshSolid(id);
    WARN("pieces: " << made.pieces.size() << ", "
                    << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s");
}

// Slice 96: toy joint recipes.
TEST_CASE("Cut into Parts: toy joint recipes", "[limbcut][recipes]")
{
    CHECK(limbcut::recipeFits(limbcut::Recipe::Butterfly, Joint::LeftShoulder));
    CHECK_FALSE(limbcut::recipeFits(limbcut::Recipe::Butterfly, Joint::LeftHip));
    CHECK(limbcut::recipeFits(limbcut::Recipe::AbCrunch, Joint::Waist));
    CHECK(limbcut::recipeFits(limbcut::Recipe::AnkleRocker, Joint::RightAnkle));
    CHECK(limbcut::recipeFits(limbcut::Recipe::DropHip, Joint::RightHip));

    Document doc;
    CommandStack stack(doc);
    const ObjectId figure = addFigure(stack, doc, trianglesOf(tFigure()));
    const PosedFinish skeleton = rig::fit(onPlate(doc, figure));
    const double whole = meshkernel::volume(doc.meshSolid(figure));

    std::vector<limbcut::Choice> choices;
    const auto add = [&](Joint j, limbcut::JointKind kind, limbcut::Recipe recipe, double radius) {
        limbcut::Choice c;
        c.joint = j;
        c.kind = kind;
        c.recipe = recipe;
        c.ball = limbcut::ballFor(radius);
        c.hinge = limbcut::hingeFor(radius, HingeKind::Single);
        choices.push_back(c);
    };
    add(Joint::LeftShoulder, limbcut::JointKind::Ball, limbcut::Recipe::Butterfly, 4.5);
    add(Joint::RightHip, limbcut::JointKind::Ball, limbcut::Recipe::DropHip, 6.0);
    add(Joint::LeftAnkle, limbcut::JointKind::Hinge, limbcut::Recipe::AnkleRocker, 3.5);
    add(Joint::Waist, limbcut::JointKind::Hinge, limbcut::Recipe::AbCrunch, 10.0);
    const limbcut::Made made = limbcut::cutIntoParts(stack, doc, figure, skeleton.points, choices, 200);

    std::set<std::string> names;
    double sum = 0.0;
    for (ObjectId id : made.pieces) {
        REQUIRE(doc.isTopLevel(id));
        names.insert(doc.find(id)->name);
        const double v = meshkernel::volume(doc.meshSolid(id));
        CHECK(v > 10.0);
        sum += v;
    }
    for (const char* block : {"Left shoulder block", "Right hip block", "Left ankle block", "Waist block"})
        CHECK(names.count(block) == 1);
    CHECK(made.pieces.size() == 1 + 4 + 4);  // the figure, four cuts, four blocks
    // Pins: the butterfly's, the drop hip's, the rocker's two, the crunch's hinge (the swivel's peg too).
    CHECK(made.extras.size() >= 5);
    CHECK(sum < whole * 1.02);
    CHECK(sum > whole * 0.85);
    // Each block is small, and next to its joint.
    for (ObjectId id : made.pieces)
        if (doc.find(id)->name == "Left shoulder block") {
            const Bounds b = doc.worldBounds(id);
            CHECK(b.size()[0] < 25.0);
            CHECK(b.center()[0] > 0.0);  // on the figure's left (+X)
        }
    stack.undo();
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{figure});

    // A recipe at the wrong joint is refused.
    choices = {};
    add(Joint::LeftKnee, limbcut::JointKind::Ball, limbcut::Recipe::Butterfly, 5.0);
    CHECK_THROWS_AS(limbcut::cutIntoParts(stack, doc, figure, skeleton.points, choices, 300), std::invalid_argument);
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{figure});
}
