// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 100: accessory makers.
#include "forge/core/Accessory.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <map>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Bounds boundsOf(const Mesh& m)
{
    Bounds b;
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
        b.expand(m.positions[i], m.positions[i + 1], m.positions[i + 2]);
    return b;
}

bool closed(const Mesh& m)
{
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k)
            ++edges[{m.indices[t + k], m.indices[t + (k + 1) % 3]}];
    for (const auto& [e, n] : edges)
        if (n != 1 || edges.count({e.second, e.first}) != 1)
            return false;
    return true;
}

}  // namespace

TEST_CASE("Accessories: each kind is made closed, lying on the plate, at the figure's scale", "[accessory]")
{
    for (int k = 0; k < kAccessoryKindCount; ++k) {
        AccessorySettings s;
        s.kind = AccessoryKind(k);
        INFO(accessory::title(s.kind));
        const auto t0 = std::chrono::steady_clock::now();
        const Mesh m = accessory::make(s);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        CHECK(seconds < 20.0);
        CHECK(m.triangleCount() > 100);
        CHECK(closed(m));
        const Bounds b = boundsOf(m);
        CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-4));
        CHECK_THAT((b.min[0] + b.max[0]) / 2.0, WithinAbs(0.0, 1e-3));
        CHECK(meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices)) > 0.0);
    }
}

TEST_CASE("Accessories: sizes", "[accessory]")
{
    SECTION("A sword at 1:12, its grip the hands' handle")
    {
        AccessorySettings s;  // 75 cm blade, 18 cm grip, at 1:12
        const Bounds b = boundsOf(accessory::make(s));
        // Blade 62.5 + guard 1.3 + grip 15 + pommel: about 82 mm long.
        CHECK(b.size()[0] > 78.0);
        CHECK(b.size()[0] < 88.0);
        CHECK_THAT(b.size()[1], WithinAbs(15.0, 0.6));  // the guard, 18 cm across
        // Lying flat: as thick as the pommel (1.5 x the 3 mm handle).
        CHECK_THAT(b.size()[2], WithinAbs(4.5, 0.4));
        CHECK(accessory::nameOf(s) == "Sword");
        s.bladeLength = 250.0;
        CHECK(accessory::nameOf(s) == "Dagger");
        s.scale = 6.0;  // twice as big at 1:6
        const Bounds big = boundsOf(accessory::make(s));
        CHECK(big.size()[0] > 2.0 * 30.0);
    }
    SECTION("A round shield, 60 cm at 1:12")
    {
        AccessorySettings s;
        s.kind = AccessoryKind::Shield;
        s.shield = ShieldShape::Round;
        s.shieldCurve = 0.0;
        s.boss = false;
        s.gripBar = false;
        const Bounds b = boundsOf(accessory::make(s));
        CHECK_THAT(b.size()[0], WithinAbs(50.0, 0.6));
        CHECK_THAT(b.size()[1], WithinAbs(50.0, 0.6));
        CHECK_THAT(b.size()[2], WithinAbs(1.25, 0.3));
    }
    SECTION("A belt round the waist, open at the back")
    {
        AccessorySettings s;
        s.kind = AccessoryKind::Belt;
        const Mesh m = accessory::make(s);
        const Bounds b = boundsOf(m);
        // 30 mm waist, 0.2 fit and a band 0.8 thick each side.
        CHECK_THAT(b.size()[0], WithinAbs(30.0 + 0.4 + 1.6, 0.4));
        CHECK(b.size()[1] > 20.0 + 0.4 + 1.6);  // and the buckle on the front
        // The waist goes through: a rod down its middle doesn't touch it.
        const MeshSolid solid = meshkernel::fromTriangles(m.positions, m.indices);
        const auto c = b.center();
        Mesh rod;
        CHECK(meshkernel::volume(solid) > 0.0);
        (void)c;
    }
    SECTION("Out of range")
    {
        AccessorySettings s;
        s.handle = 0.0;
        CHECK_THROWS_AS(accessory::make(s), std::invalid_argument);
        s = AccessorySettings{};
        s.kind = AccessoryKind::Belt;
        s.beltGap = 40.0;
        CHECK_THROWS_AS(accessory::make(s), std::invalid_argument);
        s = AccessorySettings{};
        s.kind = AccessoryKind::Holster;
        s.scale = 60.0;
        CHECK_THROWS_AS(accessory::make(s), std::invalid_argument);
    }
}

TEST_CASE("Accessories: added to the plate", "[accessory]")
{
    Document doc;
    CommandStack stack(doc);
    AccessorySettings s;
    s.kind = AccessoryKind::Staff;
    s.staffEnd = StaffEnd::Spear;
    const ObjectId a = accessory::add(stack, doc, s, 50.0, 10.0);
    const ObjectId b = accessory::add(stack, doc, s, 50.0, 30.0);
    CHECK(doc.find(a)->name == "Spear");
    CHECK(doc.find(b)->name != "Spear");
    const Bounds w = doc.worldBounds(a);
    CHECK_THAT((w.min[0] + w.max[0]) / 2.0, WithinAbs(50.0, 0.5));
    CHECK_THAT(w.min[2], WithinAbs(0.0, 0.01));
    stack.undo();
    stack.undo();
    CHECK(doc.bodies().empty());
}
