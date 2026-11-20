// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 57: the figure guide (mannequin).
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Mannequin.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

void setAngles(Mannequin& m, MJoint j, double a, double b = 0.0)
{
    m.pose[std::size_t(j)] = {a, b};
}

}  // namespace

TEST_CASE("Figure guide: proportions from the height in heads", "[mannequin]")
{
    Mannequin m;
    m.height = 160;
    m.heads = 8;
    const auto l = mannequin::landmarks(m);
    CHECK_THAT(l.top[2], WithinAbs(160, 1e-6));
    CHECK_THAT(l.chin[2], WithinAbs(140, 1e-6));  // one head down
    // Hips about halfway, knees about a quarter up, ankles near the ground.
    CHECK(l.leftHip[2] > 75);
    CHECK(l.leftHip[2] < 90);
    CHECK(l.leftKnee[2] > 35);
    CHECK(l.leftKnee[2] < 45);
    CHECK(l.leftAnkle[2] < 10);
    // Left is +X (it faces the front, -Y); the two sides mirror.
    CHECK(l.leftShoulder[0] > 0);
    CHECK_THAT(l.leftShoulder[0], WithinAbs(-l.rightShoulder[0], 1e-9));
    CHECK_THAT(l.leftWrist[2], WithinAbs(l.rightWrist[2], 1e-9));
    // Hands hang to about mid-thigh.
    CHECK(l.leftWrist[2] < l.leftHip[2]);
    CHECK(l.leftWrist[2] > l.leftKnee[2]);

    const Mesh body = mannequin::mesh(m);
    REQUIRE_FALSE(body.empty());
    const Bounds b = body.bounds();
    CHECK_THAT(b.max[2], WithinAbs(160, 0.5));
    CHECK(b.min[2] > -1.0);
    CHECK(b.min[2] < 3.0);

    // Fewer heads: a bigger head on the same height.
    m.heads = 6;
    CHECK_THAT(mannequin::landmarks(m).chin[2], WithinAbs(160 - 160.0 / 6, 1e-6));
}

TEST_CASE("Figure guide: posing", "[mannequin]")
{
    Mannequin m;
    m.height = 160;
    m.heads = 8;
    // Left arm straight out to the side: the wrist level with the shoulder, further out.
    setAngles(m, MJoint::LeftShoulder, 0, 90);
    auto l = mannequin::landmarks(m);
    CHECK_THAT(l.leftWrist[2], WithinAbs(l.leftShoulder[2], 1e-6));
    CHECK(l.leftWrist[0] > l.leftShoulder[0] + 30);
    // Straight forward instead: in front (towards -Y).
    setAngles(m, MJoint::LeftShoulder, 90, 0);
    l = mannequin::landmarks(m);
    CHECK_THAT(l.leftWrist[2], WithinAbs(l.leftShoulder[2], 1e-6));
    CHECK(l.leftWrist[1] < -30);
    // A bent knee: the ankle goes back (+Y).
    setAngles(m, MJoint::RightKnee, 90);
    l = mannequin::landmarks(m);
    CHECK(l.rightAnkle[1] > 30);
    CHECK_THAT(l.rightAnkle[2], WithinAbs(l.rightKnee[2], 1e-6));
    // Both knees bent: it comes down so its feet stay on the plate.
    setAngles(m, MJoint::LeftKnee, 60);
    setAngles(m, MJoint::RightKnee, 60);
    setAngles(m, MJoint::LeftHip, 30);
    setAngles(m, MJoint::RightHip, 30);
    l = mannequin::landmarks(m);
    CHECK(l.top[2] < 157);
    CHECK(mannequin::mesh(m).bounds().min[2] < 2.0);
    CHECK(mannequin::mesh(m).bounds().min[2] > -2.0);
    setAngles(m, MJoint::RightKnee, 90);
    setAngles(m, MJoint::RightHip, 0);
    // Turned a quarter: the forward arm points along +X; moved, everything moves.
    m.turn = 90;
    m.x = 50;
    l = mannequin::landmarks(m);
    CHECK(l.leftWrist[0] > 50 + 30);
    // The presets are all in range.
    for (int p = 0; p < mannequin::kPosePresetCount; ++p) {
        m.pose = mannequin::preset(mannequin::PosePreset(p));
        CHECK_NOTHROW(mannequin::validate(m));
    }
    // Out of range.
    setAngles(m, MJoint::LeftKnee, -20);
    CHECK_THROWS_AS(mannequin::validate(m), std::invalid_argument);
    m.pose = {};
    m.heads = 2;
    CHECK_THROWS_AS(mannequin::validate(m), std::invalid_argument);
}

TEST_CASE("Figure guide: undo and the file", "[mannequin]")
{
    Document doc;
    CommandStack stack(doc);
    int heard = 0;
    doc.addMannequinListener([&heard] { ++heard; });
    Mannequin m;
    stack.execute(std::make_unique<SetMannequinCommand>(m, "Add Figure Guide"));
    REQUIRE(doc.mannequin());
    // A run of changes with one label is one undo step.
    for (double h : {160.0, 170.0, 180.0}) {
        m.height = h;
        stack.execute(std::make_unique<SetMannequinCommand>(m, "Change Figure Guide", true));
    }
    CHECK_THAT(doc.mannequin()->height, WithinAbs(180, 1e-9));
    stack.undo();
    CHECK_THAT(doc.mannequin()->height, WithinAbs(152.4, 1e-9));
    stack.redo();
    CHECK(heard >= 3);

    Mannequin bad = m;
    bad.heads = 50;
    CHECK_THROWS(stack.execute(std::make_unique<SetMannequinCommand>(bad, "Change Figure Guide")));
    CHECK_THAT(doc.mannequin()->heads, WithinAbs(7.5, 1e-9));

    m.pose = mannequin::preset(mannequin::PosePreset::Action);
    m.turn = 30;
    m.shown = false;
    stack.execute(std::make_unique<SetMannequinCommand>(m, "Pose Figure Guide"));
    const auto path = std::filesystem::temp_directory_path() / "forge_mannequin_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::loadMannequin(path);
    std::filesystem::remove(path);
    REQUIRE(back);
    CHECK(*back == *doc.mannequin());

    stack.execute(std::make_unique<SetMannequinCommand>(std::nullopt, "Remove Figure Guide"));
    CHECK_FALSE(doc.mannequin());
    CHECK_FALSE(file::mannequinFromJson(R"({"bodies":[]})"));
    CHECK_FALSE(file::mannequinFromJson(R"({"mannequin":{"heads":1}})"));
}
