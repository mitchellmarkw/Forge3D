// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 47: Join Parts.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Join.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Primitives.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Socket.h"
#include "forge/core/Tessellator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Shape");
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId combine(CommandStack& stack, Document& doc, BooleanSpec spec)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Result");
    r.boolean = std::move(spec);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

// Moves `id` to where Join Parts puts it.
void place(CommandStack& stack, Document& doc, ObjectId id, const Placement& p)
{
    Body after = *doc.find(id);
    after.placement = p;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(id), after, "Join"));
}

MeshSolid placed(const Document& doc, ObjectId id)
{
    return meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id));
}

double overlap(const Document& doc, ObjectId a, ObjectId b)
{
    return meshkernel::volume(meshkernel::intersected({placed(doc, a), placed(doc, b)}));
}

void near(const Vec3& a, const Vec3& b, double tol = 1e-6)
{
    for (std::size_t k = 0; k < 3; ++k)
        CHECK_THAT(a[k], WithinAbs(b[k], tol));
}

}  // namespace

TEST_CASE("Join: the move puts the features together, facing each other", "[join]")
{
    join::Feature a;
    a.point = {1, 2, 3};
    a.direction = {0, 0, 1};
    join::Feature b;
    b.point = {10, 5, -3};
    b.direction = {1, 0, 0};
    const Transform t = join::mating(a, b, {});
    near(t.apply(a.point), b.point);
    near(t.applyDirection(a.direction), {-1, 0, 0});

    join::Offsets o;
    o.along = 2.0;
    o.turn = 90.0;
    const Transform u = join::mating(a, b, o);
    near(u.apply(a.point), {12, 5, -3});  // backed off along b's direction; the turn doesn't move it
    near(u.applyDirection(a.direction), {-1, 0, 0});
    // Turned a quarter round b's line compared with no turn.
    const Vec3 side = t.applyDirection({1, 0, 0}), turned = u.applyDirection({1, 0, 0});
    CHECK_THAT(side[0] * turned[0] + side[1] * turned[1] + side[2] * turned[2], WithinAbs(0.0, 1e-9));

    o = {};
    o.flip = true;
    near(join::mating(a, b, o).applyDirection(a.direction), {1, 0, 0});
    o = {};
    o.acrossX = 1.0;
    o.acrossY = 2.0;
    const Vec3 p = join::mating(a, b, o).apply(a.point);
    CHECK_THAT(p[0], WithinAbs(10.0, 1e-9));  // across, not along
    CHECK_THAT(std::hypot(p[1] - 5.0, p[2] + 3.0), WithinAbs(std::sqrt(5.0), 1e-9));
}

TEST_CASE("Join: a flat face against a flat face", "[join]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{10, 10, 10});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 50, 0, 0);
    const auto top = join::featureAt(doc, a, {2, 1, 50}, {0, 0, -1});
    REQUIRE(top);
    CHECK(top->kind == join::Feature::Kind::Flat);
    near(top->point, {0, 0, 10}, 1e-6);  // the face's middle, not where it was clicked
    near(top->direction, {0, 0, 1});
    const auto side = join::featureAt(doc, b, {100, 3, 12}, {-1, 0, 0});
    REQUIRE(side);
    CHECK(side->kind == join::Feature::Kind::Flat);
    near(side->point, {60, 0, 10}, 1e-6);
    place(stack, doc, a, join::joinedPlacement(doc, a, *top, *side, {}));
    // A's top now lies on B's +X side, centred on it.
    const Bounds box = doc.worldBounds(a);
    CHECK_THAT(box.min[0], WithinAbs(60.0, 1e-3));
    CHECK_THAT(box.max[0], WithinAbs(70.0, 1e-3));
    CHECK_THAT(box.center()[1], WithinAbs(0.0, 1e-3));
    CHECK_THAT(box.center()[2], WithinAbs(10.0, 1e-3));
}

TEST_CASE("Join: a peg into a hole, to the bottom less the gap", "[join]")
{
    Document doc;
    CommandStack stack(doc);
    // A block with a hole 6 deep from its top (z = 10).
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});
    const ObjectId drill = add(stack, doc, CylinderSpec{1.6, 10}, 0, 0, 4);
    BooleanSpec cut;
    cut.op = BooleanOp::Subtract;
    cut.operands = {block, drill};
    const ObjectId holed = combine(stack, doc, cut);
    const ObjectId peg = add(stack, doc, CylinderSpec{1.5, 5}, 40, 0, 0);

    const auto pegSide = join::featureAt(doc, peg, {50, 0, 2.5}, {-1, 0, 0});
    REQUIRE(pegSide);
    CHECK(pegSide->kind == join::Feature::Kind::Peg);
    CHECK_THAT(pegSide->size, WithinAbs(3.0, 1e-6));
    // Slanting in through the opening to the hole's wall.
    const auto hole = join::featureAt(doc, holed, {0, 0, 20}, {0.15, 0, -1});
    REQUIRE(hole);
    CHECK(hole->kind == join::Feature::Kind::Hole);
    CHECK_THAT(hole->size, WithinAbs(3.2, 1e-6));
    near(hole->point, {0, 0, 4}, 1e-6);  // its bottom
    near(hole->direction, {0, 0, 1});      // out of the opening

    join::Offsets o;
    o.along = 0.2;
    place(stack, doc, peg, join::joinedPlacement(doc, peg, *pegSide, *hole, o));
    const Bounds box = doc.worldBounds(peg);
    CHECK_THAT(box.min[2], WithinAbs(4.2, 1e-3));
    CHECK_THAT(box.max[2], WithinAbs(9.2, 1e-3));
    CHECK_THAT(box.center()[0], WithinAbs(0.0, 1e-3));
    CHECK(overlap(doc, holed, peg) < 1e-3);  // in the hole, touching nothing
}

TEST_CASE("Join: a ball into a Quick Socket; a peg seated in its socket", "[join]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});
    SocketFinish f;
    f.size = 3.0;
    f.fit = 0.1;
    Body s;
    s.id = doc.allocateId();
    s.name = "Socketed";
    s.boolean = socketSpec(doc, block, {{-4, 0, 10}, {0, 0, 1}}, f);
    stack.execute(std::make_unique<CombineCommand>(s));
    const ObjectId ball = add(stack, doc, socket::matchingPart(f), 40, 0, 0);

    const auto b = join::featureAt(doc, ball, {50, 0, 1.5}, {-1, 0, 0});
    REQUIRE(b);
    CHECK(b->kind == join::Feature::Kind::Ball);
    near(b->point, {40, 0, 1.5}, 1e-6);
    // Straight down into the socket.
    const auto cup = join::featureAt(doc, s.id, {-4, 0, 30}, {0, 0, -1});
    REQUIRE(cup);
    CHECK(cup->kind == join::Feature::Kind::Socket);
    CHECK(cup->text == "Ball socket \xC3\x98" "3 (snap-in)");
    const auto seat = socket::ballSeat(f);
    near(cup->point, {-4, 0, 10 - seat.centreDepth}, 1e-6);
    CHECK(cup->gap == 0.0);
    place(stack, doc, ball, join::joinedPlacement(doc, ball, *b, *cup, {}));
    near(doc.worldTransform(ball).apply({0, 0, 1.5}), cup->point, 1e-6);
    CHECK(overlap(doc, s.id, ball) < 1e-3);

    // A round peg socket, and its peg pushed in with the suggested gap: flush at the top.
    const ObjectId block2 = add(stack, doc, BoxSpec{20, 20, 10}, 0, 40, 0);
    SocketFinish g;
    g.kind = SocketKind::Peg;
    g.size = 3.0;
    g.depth = 4.0;
    g.fit = 0.1;
    Body s2;
    s2.id = doc.allocateId();
    s2.name = "Pegged";
    s2.boolean = socketSpec(doc, block2, {{0, 40, 10}, {0, 0, 1}}, g);
    stack.execute(std::make_unique<CombineCommand>(s2));
    const ObjectId peg = add(stack, doc, socket::matchingPart(g), 40, 40, 0);
    const auto p = join::featureAt(doc, peg, {50, 40, 2}, {-1, 0, 0});
    REQUIRE(p);
    CHECK(p->kind == join::Feature::Kind::Peg);
    const auto hole = join::featureAt(doc, s2.id, {0.5, 40, 30}, {0, 0, -1});
    REQUIRE(hole);
    CHECK(hole->kind == join::Feature::Kind::Socket);
    CHECK_THAT(hole->gap, WithinAbs(0.1, 1e-12));
    join::Offsets o;
    o.along = hole->gap;
    place(stack, doc, peg, join::joinedPlacement(doc, peg, *p, *hole, o));
    const Bounds box = doc.worldBounds(peg);
    CHECK_THAT(box.max[2], WithinAbs(10.0, 1e-3));  // its end level with the surface
    CHECK_THAT(box.min[2], WithinAbs(6.0, 1e-3));
    CHECK(overlap(doc, s2.id, peg) < 1e-3);
}

TEST_CASE("Join: a ball found on a mesh; elsewhere just the spot", "[join]")
{
    Document doc;
    CommandStack stack(doc);
    const Mesh ball = sculpt::refined(
        sculpt::welded(tessellate(primitives::buildShape(SphereSpec{3.0})), 1e-4), 0.3, 10'000'000);
    auto data = std::make_shared<MeshData>();
    data->positions = ball.positions;
    data->indices = ball.indices;
    const Bounds bb = ball.bounds();
    data->size = bb.size();
    MeshSpec m;
    m.data = data;
    m.width = bb.size()[0];
    m.depth = bb.size()[1];
    m.height = bb.size()[2];
    const ObjectId id = add(stack, doc, m, 30, 0, 0);
    const auto c = doc.worldBounds(id).center();
    const auto f = join::featureAt(doc, id, {c[0] + 20, c[1] + 0.5, c[2] + 0.3}, {-1, 0, 0});
    REQUIRE(f);
    CHECK(f->kind == join::Feature::Kind::Ball);
    CHECK_THAT(f->size, WithinRel(6.0, 0.03));
    near(f->point, {c[0], c[1], c[2]}, 0.1);

    // A mesh box: no ball, the spot.
    auto boxData = std::make_shared<MeshData>();
    boxData->positions = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1};
    boxData->indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                        1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    boxData->size = {1, 1, 1};
    MeshSpec mb;
    mb.data = boxData;
    mb.width = mb.depth = mb.height = 10;
    const ObjectId boxId = add(stack, doc, mb, -30, 0, 0);
    const auto bc = doc.worldBounds(boxId).center();
    const auto s = join::featureAt(doc, boxId, {bc[0], bc[1], 50}, {0, 0, -1});
    REQUIRE(s);
    CHECK(s->kind == join::Feature::Kind::Spot);
    near(s->direction, {0, 0, 1}, 1e-6);
    CHECK(!join::featureAt(doc, boxId, {bc[0], bc[1] + 100, 50}, {0, 0, -1}));  // missed
}
