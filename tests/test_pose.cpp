// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 51: Pose test.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Hinge.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Pose.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId addBox(CommandStack& stack, Document& doc, double z, double w, double d, double h, const std::string& name)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{w, d, h};
    b.name = name;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId addStep(CommandStack& stack, Document& doc, BooleanSpec spec, const std::string& name)
{
    Body r;
    r.id = doc.allocateId();
    r.name = name;
    r.boolean = std::move(spec);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

// A thigh (20 mm cube) with a hinge fork on top, and a shin above whose end is made into
// the tab.
struct Leg {
    Document doc;
    CommandStack stack{doc};
    ObjectId thigh = kInvalidId, shin = kInvalidId, forkStep = kInvalidId;
    HingeFinish f;
    Leg(double shinWidth = 20.0)
    {
        const ObjectId lower = addBox(stack, doc, 0, 20, 20, 20, "Thigh");
        const ObjectId upper = addBox(stack, doc, 20, shinWidth, shinWidth, 20, "Shin");
        f.link = 42;
        const emboss::SurfacePoint top{{0, 0, 20}, {0, 0, 1}};
        forkStep = addStep(stack, doc, hingeSpec(doc, lower, top, f), "Thigh");
        const hinge::Half h = hinge::otherHalf(f, top.point, top.normal);
        addStep(stack, doc, hingeSpec(doc, upper, {h.point, h.normal}, h.finish), "Shin");
        thigh = history::objectOf(doc, forkStep);
        shin = history::objectOf(doc, upper);
    }
    std::map<ObjectId, double> sizes() const
    {
        std::map<ObjectId, double> out;
        for (ObjectId id : doc.topLevelIds())
            out[id] = meshkernel::volume(doc.meshSolid(id));
        return out;
    }
    std::map<ObjectId, Bounds> bounds() const
    {
        std::map<ObjectId, Bounds> out;
        for (ObjectId id : doc.topLevelIds())
            out[id] = doc.worldBounds(id);
        return out;
    }
    std::vector<pose::PosedPart> parts() const
    {
        std::vector<pose::PosedPart> out;
        for (ObjectId id : doc.topLevelIds())
            out.push_back({id, meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id)), Transform{}});
        return out;
    }
};

void near(const Vec3& a, const Vec3& b, double tol = 1e-9)
{
    for (int i = 0; i < 3; ++i)
        CHECK_THAT(a[i], WithinAbs(b[i], tol));
}

}  // namespace

TEST_CASE("Pose: a hinge's joint, from its fork to the part made into its tab", "[pose]")
{
    Leg leg(16.0);  // the shin smaller than the thigh
    const auto joints = pose::findJoints(leg.doc);
    REQUIRE(joints.size() == 1);
    const pose::Joint& j = joints[0];
    CHECK(j.kind == pose::JointKind::Hinge);
    CHECK(j.holder == leg.thigh);
    CHECK(j.other == leg.shin);
    near(j.pivot, {0, 0, 20.0 - hinge::pinDepth(leg.f)});
    CHECK(j.name == "Hinge in Thigh");

    // A printed pin through the joint rides with the thigh.
    Body pin = hinge::pieceBody(hinge::pieces(leg.f, {0, 0, 20}, {0, 0, 1}, false)[0].mesh, "Pin");
    pin.id = leg.doc.allocateId();
    leg.stack.execute(std::make_unique<AddBodyCommand>(pin));

    // The bigger part is held still: the shin turns from it.
    const pose::Rig rig = pose::build(joints, leg.sizes(), leg.bounds());
    CHECK(rig.hold == leg.thigh);
    CHECK(rig.parent.at(leg.shin) == leg.thigh);
    CHECK(rig.rides.at(pin.id) == leg.thigh);
    // Or the shin, if asked.
    CHECK(pose::build(joints, leg.sizes(), leg.bounds(), leg.shin).parent.at(leg.thigh) == leg.shin);

    // Bent 90 degrees, the shin turns round the pin; nothing else moves.
    const auto moved = pose::transforms(joints, rig, {{j.step, {90.0, 0.0, 0.0}}});
    REQUIRE(moved.size() == 1);
    const Transform& t = moved.at(leg.shin);
    near(t.apply(j.pivot), j.pivot);
    const Vec3 up{j.pivot[0], j.pivot[1], j.pivot[2] + 10.0};
    const Vec3 to = t.apply(up);
    CHECK_THAT(to[2], WithinAbs(j.pivot[2], 1e-9));  // now lying flat
    CHECK(pose::transforms(joints, rig, {}).empty());  // straight: the build pose
}

TEST_CASE("Pose: parts that hit are found, and how far a joint turns before they do", "[pose]")
{
    // A narrow shin (just wider than the tab) on a wide thigh.
    Leg leg(5.0);
    const auto joints = pose::findJoints(leg.doc);
    REQUIRE(joints.size() == 1);
    const pose::Rig rig = pose::build(joints, leg.sizes(), leg.bounds());
    auto parts = leg.parts();
    CHECK(pose::collisions(parts).empty());  // as built, nothing hits

    const auto posedAt = [&](double angle) {
        const auto moved = pose::transforms(joints, rig, {{joints[0].step, {angle, 0, 0}}});
        auto p = parts;
        for (auto& part : p)
            if (moved.count(part.id))
                part.moved = moved.at(part.id);
        return p;
    };
    const auto hits = pose::collisions(posedAt(90));
    REQUIRE(hits.size() == 1);
    CHECK(((hits[0].a == leg.thigh && hits[0].b == leg.shin) || (hits[0].a == leg.shin && hits[0].b == leg.thigh)));
    CHECK(hits[0].volume > 1.0);
    CHECK_FALSE(hits[0].overlap.indices.empty());

    const auto r = pose::range(joints, rig, {}, 0, parts);
    CHECK(r[0] <= 0.0);
    CHECK(r[1] >= 0.0);
    CHECK(r[1] < 90.0);
    CHECK(pose::collisions(posedAt(r[0])).empty());
    CHECK(pose::collisions(posedAt(r[1])).empty());
    CHECK_FALSE(pose::collisions(posedAt(r[1] + 2.0)).empty());
    CHECK_FALSE(pose::collisions(posedAt(r[0] - 2.0)).empty());
}

TEST_CASE("Pose: a ball's bend, sideways bend and twist", "[pose]")
{
    pose::Joint ball;
    ball.kind = pose::JointKind::Ball;
    ball.pivot = {3, -2, 7};
    ball.axis = {0, 0, 1};
    ball.across = {1, 0, 0};
    const std::array<double, 3> a{20.0, -30.0, 40.0};
    const Transform t = pose::turn(ball, a);
    near(t.apply(ball.pivot), ball.pivot);
    const auto back = pose::ballAngles(ball, t);
    for (int i = 0; i < 3; ++i)
        CHECK_THAT(back[i], WithinAbs(a[i], 1e-9));
}

TEST_CASE("Pose: a double hinge's link turns from one fork, the other part from the link", "[pose]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId lower = addBox(stack, doc, 0, 20, 20, 20, "Upper arm");
    const ObjectId upper = addBox(stack, doc, 21, 20, 20, 20, "Forearm");
    HingeFinish f;
    f.kind = HingeKind::Double;
    f.gap = 1.0;
    const emboss::SurfacePoint top{{0, 0, 20}, {0, 0, 1}};
    const ObjectId fork1 = addStep(stack, doc, hingeSpec(doc, lower, top, f), "Upper arm");
    const hinge::Half h = hinge::otherHalf(f, top.point, top.normal);
    const ObjectId fork2 = addStep(stack, doc, hingeSpec(doc, upper, {h.point, h.normal}, h.finish), "Forearm");
    const auto pieces = hinge::pieces(f, top.point, top.normal, true);
    Body link = hinge::pieceBody(pieces[0].mesh, "Link");
    link.id = doc.allocateId();
    stack.execute(std::make_unique<AddBodyCommand>(link));

    const auto joints = pose::findJoints(doc);
    REQUIRE(joints.size() == 2);
    for (const auto& j : joints)
        CHECK(j.other == link.id);  // the link is round both pins (and bigger than a pin)
    std::map<ObjectId, double> sizes{{fork1, 8000}, {fork2, 8000}, {link.id, 50}};
    std::map<ObjectId, Bounds> bounds;
    for (ObjectId id : doc.topLevelIds())
        bounds[id] = doc.worldBounds(id);
    const pose::Rig rig = pose::build(joints, sizes, bounds, fork1);
    CHECK(rig.parent.at(link.id) == fork1);
    CHECK(rig.parent.at(fork2) == link.id);
    // 45 at each pin: the forearm turns 90 in all.
    std::map<ObjectId, std::array<double, 3>> angles;
    for (const auto& j : joints)
        angles[j.step] = {45.0, 0, 0};
    const auto moved = pose::transforms(joints, rig, angles);
    const Vec3 a = moved.at(fork2).applyDirection({0, 0, 1});
    CHECK_THAT(std::abs(a[2]), WithinAbs(0.0, 1e-9));
}

TEST_CASE("Pose: poses are saved by name, undone, and kept in the file", "[pose]")
{
    Leg leg;
    const auto joints = pose::findJoints(leg.doc);
    SavedPose kneel{"Kneel", leg.thigh, {{joints[0].step, {75.0, 0.0, 0.0}}}};
    SavedPose stand{"Stand", leg.thigh, {}};
    leg.stack.execute(std::make_unique<SetPosesCommand>(std::vector{kneel, stand}, "Save Pose"));
    CHECK(leg.doc.poses().size() == 2);
    CHECK_THROWS(leg.doc.setPoses({kneel, kneel}));
    leg.stack.undo();
    CHECK(leg.doc.poses().empty());
    leg.stack.redo();

    const auto path = std::filesystem::temp_directory_path() / "forge3d-test-poses.f3d";
    file::save(leg.doc, path, "test");
    const auto back = file::loadPoses(path);
    REQUIRE(back.size() == 2);
    CHECK(back[0] == kneel);
    CHECK(back[1] == stand);
    std::filesystem::remove(path);
    CHECK(file::posesFromJson(R"({"poses":[{"name":""},{"name":"A","joints":[{"step":3,"angles":[1,2,3]}]}]})").size() == 1);
}
