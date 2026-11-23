// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 78: instruction sheets (the exploded view)
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Illustration.h"
#include "forge/core/PartsList.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

struct Scene {
    Document doc;
    CommandStack stack{doc};
    ObjectId add(const PrimitiveSpec& spec, const std::string& name, double x, double y, double z)
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = spec;
        b.name = name;
        b.placement.x = x;
        b.placement.y = y;
        b.placement.z = z;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        return b.id;
    }
    // A simple figure: body, head, two arms, two legs.
    void figure()
    {
        add(BoxSpec{10, 6, 14}, "Torso", 0, 0, 16);
        add(SphereSpec{4}, "Head", 0, 0, 30.5);
        add(CylinderSpec{1.6, 12}, "Left arm", 7, 0, 17);
        add(CylinderSpec{1.6, 12}, "Right arm", -7, 0, 17);
        add(BoxSpec{4, 5, 16}, "Left leg", 2.6, 0, 0);
        add(BoxSpec{4, 5, 16}, "Right leg", -2.6, 0, 0);
    }
};

// FORGE_DUMP=folder writes the picture there (to look at).
void dump(const illustrate::Picture& p, const std::string& name)
{
    const char* folder = std::getenv("FORGE_DUMP");
    if (!folder)
        return;
    std::ofstream f(std::string(folder) + "/" + name + ".ppm", std::ios::binary);
    f << "P6\n" << p.width << " " << p.height << "\n255\n";
    for (std::uint32_t c : p.pixels) {
        const double a = ((c >> 24) & 0xFF) / 255.0;
        const auto mix = [&](int v) { return char(int(v * a + 255 * (1 - a) + 0.5)); };
        f << mix(int((c >> 16) & 0xFF)) << mix(int((c >> 8) & 0xFF)) << mix(int(c & 0xFF));
    }
    std::ofstream labels(std::string(folder) + "/" + name + ".txt");
    labels << p.balloon << "\n";
    for (const auto& l : p.labels)
        labels << l.number << " " << l.ax << " " << l.ay << " " << l.bx << " " << l.by << "\n";
}

bool filledAt(const illustrate::Picture& p, double x, double y)
{
    const int xi = int(x), yi = int(y);
    if (xi < 0 || yi < 0 || xi >= p.width || yi >= p.height)
        return false;
    return (p.pixels[std::size_t(yi) * std::size_t(p.width) + std::size_t(xi)] >> 24) > 0;
}

}  // namespace

TEST_CASE("Illustration: an exploded figure, every part numbered", "[illustrate]")
{
    Scene s;
    s.figure();
    parts::Settings set;
    const auto rows = parts::rows(s.doc, s.doc.topLevelIds(), set);
    REQUIRE(rows.size() == 6);
    if (const char* folder = std::getenv("FORGE_DUMP"))
        file::save(s.doc, std::string(folder) + "/figure.f3d", "test");
    const auto still = illustrate::exploded(s.doc, rows, 0.0);
    const auto apart = illustrate::exploded(s.doc, rows, 0.6);
    REQUIRE(apart.size() == 6);
    // The head moves up, the legs down, the arms out.
    const auto top = [](const illustrate::Item& it) {
        float z = -1e9f;
        for (std::size_t i = 2; i < it.mesh.positions.size(); i += 3)
            z = std::max(z, it.mesh.positions[i]);
        return z;
    };
    CHECK(top(apart[1]) > top(still[1]) + 3.0f);
    CHECK(top(apart[4]) < top(still[4]) - 3.0f);

    for (auto view : {illustrate::View::Front, illustrate::View::ThreeQuarter, illustrate::View::Top}) {
        const auto pic = illustrate::render(apart, view, 600, 500);
        dump(pic, std::string("figure-") + illustrate::title(view));
        REQUIRE(pic.pixels.size() == 600u * 500u);
        std::size_t drawn = 0;
        for (auto c : pic.pixels)
            drawn += (c >> 24) > 0;
        CHECK(drawn > 600u * 500u / 50u);
        if (view == illustrate::View::Top)
            continue;  // from above, parts hide one another
        REQUIRE(pic.labels.size() == 6);
        for (const auto& l : pic.labels) {
            CHECK(filledAt(pic, l.ax, l.ay));          // the dot is on the part
            CHECK_FALSE(filledAt(pic, l.bx, l.by));    // the balloon isn't
            CHECK(l.bx >= pic.balloon);
            CHECK(l.by >= pic.balloon);
            CHECK(l.bx <= pic.width - pic.balloon);
            CHECK(l.by <= pic.height - pic.balloon);
            for (const auto& o : pic.labels)
                if (&o != &l)
                    CHECK(std::hypot(o.bx - l.bx, o.by - l.by) >= 2 * pic.balloon);
        }
    }
}

TEST_CASE("Illustration: a joined shape's parts, and bad sizes", "[illustrate]")
{
    Scene s;
    const ObjectId a = s.add(BoxSpec{10, 10, 10}, "A", 0, 0, 0);
    const ObjectId b = s.add(BoxSpec{10, 10, 10}, "B", 20, 0, 0);
    Body group;
    group.id = s.doc.allocateId();
    group.name = "Kit";
    group.boolean = BooleanSpec{};
    group.boolean->op = BooleanOp::Group;
    group.boolean->operands = {a, b};
    s.stack.execute(std::make_unique<CombineCommand>(group));
    parts::Settings set;
    set.inside = true;
    const auto rows = parts::rows(s.doc, s.doc.topLevelIds(), set);
    const auto items = illustrate::exploded(s.doc, rows, 0.5);
    REQUIRE(items.size() == 2);  // the two parts, not the group
    CHECK(items[0].number == 1);
    CHECK(items[1].number == 2);
    CHECK_THROWS_AS(illustrate::render(items, illustrate::View::Front, 10, 10), std::invalid_argument);
    CHECK_THROWS_AS(illustrate::exploded(s.doc, rows, -1.0), std::invalid_argument);
    const auto empty = illustrate::render({}, illustrate::View::Front, 100, 100);
    CHECK(empty.labels.empty());
}
