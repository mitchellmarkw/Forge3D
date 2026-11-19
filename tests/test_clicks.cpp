// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 53: ratchet and click joints.
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

#include <cmath>
#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

MeshSolid solidOf(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

MeshSolid cutBlock(const std::vector<Mesh>& cuts, double size = 20.0)
{
    Mesh b;
    const float h = float(size / 2.0), top = float(size);
    b.positions = {-h, -h, 0, h, -h, 0, -h, h, 0, h, h, 0, -h, -h, top, h, -h, top, -h, h, top, h, h, top};
    b.indices = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
    std::vector<MeshSolid> tools;
    for (const Mesh& c : cuts)
        tools.push_back(solidOf(c));
    return meshkernel::subtracted(solidOf(b), tools);
}

double overlap(const MeshSolid& a, const MeshSolid& b) { return meshkernel::volume(meshkernel::intersected({a, b})); }

const Vec3 kTop{0, 0, 20}, kUp{0, 0, 1};

// A ball (true radius at its points: just under it between them).
MeshSolid sphere(double r, double z)
{
    const int rings = 48, segs = 96;
    Mesh m;
    const auto push = [&m](double x, double y, double zz) {
        m.positions.insert(m.positions.end(), {float(x), float(y), float(zz)});
    };
    push(0, 0, z - r);
    for (int i = 1; i < rings; ++i) {
        const double phi = -1.5707963267948966 + 3.141592653589793 * i / rings;
        for (int k = 0; k < segs; ++k) {
            const double th = 6.283185307179586 * k / segs;
            push(r * std::cos(phi) * std::cos(th), r * std::cos(phi) * std::sin(th), z + r * std::sin(phi));
        }
    }
    const auto top = std::uint32_t(1 + (rings - 1) * segs);
    push(0, 0, z + r);
    const auto at = [](int ring, int k) { return std::uint32_t(1 + (ring - 1) * segs + (k % segs)); };
    for (int k = 0; k < segs; ++k)
        m.indices.insert(m.indices.end(), {0u, at(1, k + 1), at(1, k)});
    for (int i = 1; i < rings - 1; ++i)
        for (int k = 0; k < segs; ++k)
            m.indices.insert(m.indices.end(),
                             {at(i, k), at(i, k + 1), at(i + 1, k + 1), at(i, k), at(i + 1, k + 1), at(i + 1, k)});
    for (int k = 0; k < segs; ++k)
        m.indices.insert(m.indices.end(), {top, at(rings - 1, k), at(rings - 1, k + 1)});
    return solidOf(m);
}

}  // namespace

TEST_CASE("Clicks: a hinge's ridges sit in the grooves every click, and press in between", "[clicks]")
{
    HingeFinish f;
    f.barrel = 6.0;
    f.clicks = 8;        // every 45 degrees
    f.clickDepth = 0.4;  // fit 0.1: 0.3 mm to squeeze past
    const MeshSolid fork = cutBlock(hinge::forkCutters(f, kTop, kUp));
    const auto parts = hinge::pieces(f, kTop, kUp, true);
    const MeshSolid tab = solidOf(parts[0].mesh);
    pose::Joint j;
    j.kind = pose::JointKind::Hinge;
    j.pivot = {0, 0, 20.0 - hinge::pinDepth(f)};
    j.axis = hinge::pinAxis(f, kUp);
    const auto at = [&](double degrees) {
        return overlap(fork, meshkernel::transformed(tab, pose::turn(j, {degrees, 0, 0})));
    };
    CHECK(at(0) < 1e-4);
    CHECK(at(45) < 1e-4);
    CHECK(at(-90) < 1e-4);
    CHECK(at(90) < 1e-4);
    CHECK(at(22.5) > 1e-3);  // between clicks: the ridges press on the arms
    CHECK(at(-67.5) > 1e-3);
    CHECK(hinge::describe(f) == "Hinge fork 3 \xC3\x97 \xC3\x98" "6, printed pin \xC3\x98" "2, 8 clicks");
}

TEST_CASE("Clicks: a swivel's peg clicks round in its bore", "[clicks]")
{
    HingeFinish f;
    f.kind = HingeKind::Swivel;
    f.width = 5.0;
    f.barrel = 6.0;
    f.clicks = 8;
    const MeshSolid bore = cutBlock(hinge::forkCutters(f, kTop, kUp));
    const MeshSolid peg = solidOf(hinge::pieces(f, kTop, kUp, true)[0].mesh);
    pose::Joint j;
    j.kind = pose::JointKind::Swivel;
    j.pivot = kTop;
    j.axis = kUp;
    const auto at = [&](double degrees) {
        return overlap(bore, meshkernel::transformed(peg, pose::turn(j, {degrees, 0, 0})));
    };
    CHECK(at(0) < 1e-4);
    CHECK(at(45) < 1e-4);
    CHECK(at(22.5) > 1e-3);
}

TEST_CASE("Clicks: the settings are checked", "[clicks]")
{
    HingeFinish f;
    f.clicks = 12;
    f.clickDepth = 0.1;  // no more than the fit: it wouldn't click
    CHECK_THROWS_AS(hinge::check(f), std::invalid_argument);
    f.clickDepth = 0.4;
    f.clicks = 36;  // too many for a 5 mm end
    CHECK_THROWS_AS(hinge::check(f), std::invalid_argument);
    f.clicks = 40;
    CHECK_THROWS_AS(hinge::check(f), std::invalid_argument);
    SocketFinish s;
    s.ridges = 6;
    s.ridgeHeight = 0.05;  // less than the fit
    CHECK_THROWS_AS(socket::check(s), std::invalid_argument);
}

TEST_CASE("Ridges: a ball socket's ridges press on the ball", "[clicks]")
{
    SocketFinish s;
    s.size = 6.0;
    const auto ballIn = [&](const SocketFinish& f) {
        const auto seat = socket::ballSeat(f);
        return overlap(cutBlock(socket::cutters(f, kTop, kUp)), sphere(f.size / 2.0, 20.0 - seat.centreDepth));
    };
    const double plain = ballIn(s);
    s.ridges = 6;
    s.ridgeHeight = 0.25;
    const double ridged = ballIn(s);
    CHECK(plain < 0.05);
    CHECK(ridged > plain + 0.05);
    CHECK(socket::describe(s) == "Ball socket \xC3\x98" "6 (snap-in), 6 ridges");
}

TEST_CASE("Clicks and ridges are kept in the file", "[clicks]")
{
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Box 1";
    box.spec = BoxSpec{20, 20, 20};
    stack.execute(std::make_unique<AddBodyCommand>(box));
    HingeFinish f;
    f.clicks = 10;
    f.clickDepth = 0.35;
    Body h;
    h.id = doc.allocateId();
    h.name = "Hinged";
    h.boolean = hingeSpec(doc, box.id, {kTop, kUp}, f);
    stack.execute(std::make_unique<CombineCommand>(h));
    SocketFinish s;
    s.ridges = 5;
    s.ridgeHeight = 0.3;
    Body so;
    so.id = doc.allocateId();
    so.name = "Socketed";
    so.boolean = socketSpec(doc, h.id, {{5, 0, 0}, {1, 0, 0}}, s);
    stack.execute(std::make_unique<CombineCommand>(so));

    const auto path = std::filesystem::temp_directory_path() / "forge3d-test-clicks.f3d";
    file::save(doc, path, "test");
    Document back;
    back.replaceAll(file::load(path));
    CHECK(*back.find(h.id)->boolean->hinge == f);
    CHECK(*back.find(so.id)->boolean->socket == s);
    std::filesystem::remove(path);
}

TEST_CASE("Clicks: Pose Test rests a joint with clicks on its clicks", "[clicks]")
{
    pose::Joint j;
    j.kind = pose::JointKind::Hinge;
    j.clicks = 8;
    CHECK_THAT(pose::nearestClick(j, 30.0), WithinAbs(45.0, 1e-12));
    CHECK_THAT(pose::nearestClick(j, -100.0), WithinAbs(-90.0, 1e-12));
    j.clicks = 0;
    CHECK_THAT(pose::nearestClick(j, 30.0), WithinAbs(30.0, 1e-12));

    // Found from the document, with its clicks.
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Thigh";
    box.spec = BoxSpec{20, 20, 20};
    stack.execute(std::make_unique<AddBodyCommand>(box));
    HingeFinish f;
    f.clicks = 8;
    Body h;
    h.id = doc.allocateId();
    h.name = "Thigh";
    h.boolean = hingeSpec(doc, box.id, {kTop, kUp}, f);
    stack.execute(std::make_unique<CombineCommand>(h));
    const auto joints = pose::findJoints(doc);
    REQUIRE(joints.size() == 1);
    CHECK(joints[0].clicks == 8);
}
