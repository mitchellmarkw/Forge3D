// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 60: the panel-line cutter.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/PanelLines.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

// Points every `step` from a to b on a flat top at height z.
PanelLine straight(double x0, double y0, double x1, double y1, double z, double step = 0.5)
{
    PanelLine l;
    const double len = std::hypot(x1 - x0, y1 - y0);
    const int n = std::max(1, int(std::lround(len / step)));
    for (int k = 0; k <= n; ++k) {
        const double t = double(k) / n;
        l.points.push_back({{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z}, {0, 0, 1}});
    }
    return l;
}

double cutFrom(Document& doc, CommandStack& stack, ObjectId box, const PanelLineFinish& f)
{
    const double before = meshkernel::volume(doc.meshSolid(box));
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Lined");
    r.boolean = panelLineSpec(doc, box, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    const double removed = before - meshkernel::volume(doc.meshSolid(r.id));
    stack.undo();
    return removed;
}

}  // namespace

TEST_CASE("Panel lines: a straight groove of each shape", "[panel]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Plate");
    PanelLineFinish f;
    f.width = 0.6;
    f.depth = 0.5;
    f.lines = {straight(-8, 0, 8, 0, 10)};
    const double length = 16.0 + 2 * 0.32;  // each end reaches on half the width
    f.profile = GrooveProfile::V;
    CHECK_THAT(cutFrom(doc, stack, box, f), WithinRel(0.6 * 0.5 / 2 * length, 0.05));
    f.profile = GrooveProfile::Square;
    CHECK_THAT(cutFrom(doc, stack, box, f), WithinRel(0.6 * 0.5 * length, 0.05));
    f.profile = GrooveProfile::Round;
    CHECK_THAT(cutFrom(doc, stack, box, f), WithinRel(std::numbers::pi * 0.3 * 0.5 / 2 * length, 0.06));
    CHECK(panel::describe(f) == "1 panel line, Round 0.6 \xC3\x97 0.5 mm");
}

TEST_CASE("Panel lines: a corner, a closed loop, and round a curve", "[panel]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Plate");
    PanelLineFinish f;
    f.profile = GrooveProfile::Square;
    // An L: two legs of 6 mm, with no gap at the corner.
    PanelLine l = straight(-6, 0, 0, 0, 10);
    const PanelLine leg = straight(0, 0, 0, 6, 10);
    l.points.insert(l.points.end(), leg.points.begin() + 1, leg.points.end());
    f.lines = {l};
    const double section = f.width * f.depth;
    const double removed = cutFrom(doc, stack, box, f);
    CHECK(removed > section * 12.0 * 0.97);
    CHECK(removed < section * 13.0);

    // A closed square loop, 10 mm a side: four corners, no ends.
    PanelLine loop;
    for (const auto& [x0, y0, x1, y1] : std::vector<std::array<double, 4>>{
             {-5, -5, 5, -5}, {5, -5, 5, 5}, {5, 5, -5, 5}, {-5, 5, -5, -5}}) {
        const PanelLine side = straight(x0, y0, x1, y1, 10);
        loop.points.insert(loop.points.end(), side.points.begin(), side.points.end() - 1);
    }
    loop.closed = true;
    f.lines = {loop};
    CHECK_THAT(cutFrom(doc, stack, box, f), WithinRel(section * 40.0, 0.05));
    CHECK(panel::describe(f) == "1 panel line, Square 0.6 \xC3\x97 0.5 mm");

    // Round a ball: points over the top, each with its own way out. The groove keeps
    // its depth all the way.
    const ObjectId ball = add(stack, doc, SphereSpec{10}, "Ball");
    PanelLine arc;
    for (int k = 0; k <= 40; ++k) {
        const double a = std::numbers::pi * (0.25 + 0.5 * k / 40.0);
        const std::array<double, 3> n{std::cos(a), 0, std::sin(a)};
        arc.points.push_back({{10 * n[0], 0, 10 + 10 * n[2]}, n});
    }
    f.lines = {arc};
    const double arcLength = 10.0 * std::numbers::pi / 2;
    CHECK_THAT(cutFrom(doc, stack, ball, f), WithinRel(section * arcLength, 0.12));
}

TEST_CASE("Panel lines: saved, and checked", "[panel]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, "Plate");
    PanelLineFinish f;
    f.lines = {straight(-8, 0, 8, 0, 10), straight(0, -8, 0, 8, 10)};
    Body r;
    r.id = doc.allocateId();
    r.name = "Lined";
    r.boolean = panelLineSpec(doc, box, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    CHECK(history::describe(doc, r.id) == "2 panel lines, V 0.6 \xC3\x97 0.5 mm");
    const auto path = std::filesystem::temp_directory_path() / "forge_panel_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    const auto found = std::find_if(back.begin(), back.end(), [&](const Body& b) { return b.id == r.id; });
    REQUIRE(found != back.end());
    REQUIRE(found->boolean->panelLines);
    CHECK(*found->boolean->panelLines == *doc.find(r.id)->boolean->panelLines);

    PanelLineFinish bad = f;
    bad.width = 0;
    CHECK_THROWS_AS(panel::check(bad), std::invalid_argument);
    bad = f;
    bad.lines.front().points.resize(1);
    CHECK_THROWS_AS(panel::check(bad), std::invalid_argument);
}
