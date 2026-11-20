// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 56: rescale a design, keeping joint sizes.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x, const std::string& name,
             bool hardware = false)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    b.placement.x = x;
    b.hardware = hardware;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

void apply(CommandStack& stack, Document& doc, std::vector<Body> after)
{
    std::vector<Body> before;
    for (const Body& b : after)
        before.push_back(*doc.find(b.id));
    stack.execute(std::make_unique<EditBodiesCommand>(std::move(before), std::move(after), "Rescale"));
}

SocketFinish ball()
{
    SocketFinish f;
    f.kind = SocketKind::Ball;
    f.size = 6.0;
    f.fit = 0.1;
    return f;
}

}  // namespace

TEST_CASE("Rescale: the design shrinks together, about its bottom middle", "[rescale]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{30, 30, 60}, -40, "Body");
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 10}, 40, "Prop");
    apply(stack, doc, rescaledDesign(doc, {a, b}, 2.0 / 3.0, {0, 0, 0}, {}));
    const Bounds ba = doc.worldBounds(a), bb = doc.worldBounds(b);
    CHECK_THAT(ba.max[2] - ba.min[2], WithinAbs(40.0, 1e-4));
    CHECK_THAT(ba.min[2], WithinAbs(0.0, 1e-4));            // still on the plate
    CHECK_THAT(ba.center()[0], WithinAbs(-40.0 * 2 / 3, 1e-4));  // closer in
    CHECK_THAT(bb.center()[0], WithinAbs(40.0 * 2 / 3, 1e-4));
    CHECK_THAT(bb.max[0] - bb.min[0], WithinAbs(20.0 / 3, 1e-4));
    stack.undo();
    CHECK_THAT(doc.worldBounds(a).max[2], WithinAbs(60.0, 1e-4));
}

TEST_CASE("Rescale: joint parts keep their size and move with the design", "[rescale]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId body = add(stack, doc, BoxSpec{20, 20, 40}, 0, "Torso");
    const ObjectId peg = add(stack, doc, CylinderSpec{1.5, 6}, 30, "Peg", true);
    CHECK(jointCount(doc, {body, peg}) == 1);

    ScaleOptions keep;
    keep.keepJoints = true;
    apply(stack, doc, rescaledDesign(doc, {body, peg}, 0.5, {0, 0, 0}, keep));
    const Bounds p = doc.worldBounds(peg);
    CHECK_THAT(p.max[0] - p.min[0], WithinAbs(3.0, 1e-4));  // the same size
    CHECK_THAT(p.max[2] - p.min[2], WithinAbs(6.0, 1e-4));
    CHECK_THAT(p.center()[0], WithinAbs(15.0, 1e-4));        // but closer in
    CHECK_THAT(p.min[2], WithinAbs(0.0, 1e-4));
    CHECK_THAT(doc.worldBounds(body).max[2], WithinAbs(20.0, 1e-4));
    stack.undo();

    // Without the option it shrinks like everything else.
    apply(stack, doc, rescaledDesign(doc, {body, peg}, 0.5, {0, 0, 0}, {}));
    CHECK_THAT(doc.worldBounds(peg).max[2], WithinAbs(3.0, 1e-4));
}

TEST_CASE("Rescale: a joint part inside a combined shape, and a socket step", "[rescale]")
{
    Document doc;
    CommandStack stack(doc);
    // A limb with a peg merged on top, then a ball socket cut in its side.
    const ObjectId limb = add(stack, doc, BoxSpec{20, 20, 40}, 0, "Limb");
    Body pegBody;
    pegBody.id = doc.allocateId();
    pegBody.name = "Peg";
    pegBody.spec = CylinderSpec{2, 8};
    pegBody.placement.z = 38;
    pegBody.hardware = true;
    stack.execute(std::make_unique<AddBodyCommand>(pegBody));
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Limb with peg";
    merged.boolean = BooleanSpec{};
    merged.boolean->op = BooleanOp::Merge;
    merged.boolean->operands = {limb, pegBody.id};
    stack.execute(std::make_unique<CombineCommand>(merged));
    Body socketed;
    socketed.id = doc.allocateId();
    socketed.name = "Socketed";
    socketed.boolean = socketSpec(doc, merged.id, {{10, 0, 20}, {1, 0, 0}}, ball());
    stack.execute(std::make_unique<CombineCommand>(socketed));
    CHECK(jointCount(doc, {socketed.id}) == 2);

    ScaleOptions keep;
    keep.keepJoints = true;
    apply(stack, doc, rescaledDesign(doc, {socketed.id}, 0.5, {0, 0, 0}, keep));
    const Body* s = doc.find(socketed.id);
    CHECK_THAT(s->boolean->socket->size, WithinAbs(6.0, 1e-9));  // the socket keeps its size
    CHECK_THAT(s->boolean->planePoint[0], WithinAbs(5.0, 1e-6));  // on the side, which moved in
    CHECK_THAT(s->boolean->planePoint[2], WithinAbs(10.0, 1e-6));
    // The peg: still 4 across and 8 tall, its base on the limb's top (now 20 up).
    const Bounds whole = doc.worldBounds(socketed.id);
    CHECK_THAT(whole.max[2], WithinAbs(19.0 + 8.0, 1e-3));
    const Bounds peg = doc.worldBounds(pegBody.id);
    CHECK_THAT(peg.max[0] - peg.min[0], WithinAbs(4.0, 1e-3));
    CHECK_THAT(peg.min[2], WithinAbs(19.0, 1e-3));
    CHECK(meshkernel::volume(doc.meshSolid(socketed.id)) > 0.0);

    stack.undo();
    apply(stack, doc, rescaledDesign(doc, {socketed.id}, 0.5, {0, 0, 0}, {}));
    CHECK_THAT(doc.find(socketed.id)->boolean->socket->size, WithinAbs(3.0, 1e-9));
}

TEST_CASE("Rescale: wall thickness can be kept; joint parts are saved", "[rescale]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId peg = add(stack, doc, CylinderSpec{1.5, 6}, 30, "Peg", true);
    const auto path = std::filesystem::temp_directory_path() / "forge_rescale_test.f3d";
    file::save(doc, path, "test");
    const std::vector<Body> back = file::load(path);
    REQUIRE(back.size() == 1);
    CHECK(back.front().hardware);
    std::filesystem::remove(path);

    // A hollowed shape: its walls keep their thickness if asked.
    const ObjectId box = add(stack, doc, BoxSpec{30, 30, 30}, -30, "Box");
    Body shell;
    shell.id = doc.allocateId();
    shell.name = "Hollow";
    shell.boolean = BooleanSpec{};
    shell.boolean->op = BooleanOp::Shell;
    shell.boolean->operands = {box};
    shell.boolean->shell = ShellFinish{};
    stack.execute(std::make_unique<CombineCommand>(shell));
    ScaleOptions walls;
    walls.keepWalls = true;
    apply(stack, doc, rescaledDesign(doc, {shell.id}, 0.5, {0, 0, 0}, walls));
    CHECK_THAT(doc.find(shell.id)->boolean->shell->thickness, WithinAbs(2.0, 1e-9));
    stack.undo();
    apply(stack, doc, rescaledDesign(doc, {shell.id}, 0.5, {0, 0, 0}, {}));
    CHECK_THAT(doc.find(shell.id)->boolean->shell->thickness, WithinAbs(1.0, 1e-9));

    // Marking it again through an edit: undoable.
    Body after = *doc.find(peg);
    after.hardware = false;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(peg), after, "Not a joint part"));
    CHECK_FALSE(doc.find(peg)->hardware);
    stack.undo();
    CHECK(doc.find(peg)->hardware);
}
