// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 79: sewing patterns
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Paint.h"
#include "forge/core/Sewing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <stdexcept>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

struct Grid {
    std::vector<float> positions;
    std::vector<std::uint32_t> indices;
};

// An open tube standing up (no ends), its triangles facing out.
Grid tube(double r, double h, int around, int up)
{
    Grid g;
    for (int j = 0; j <= up; ++j)
        for (int i = 0; i < around; ++i) {
            const double a = 2 * std::numbers::pi * i / around;
            g.positions.insert(g.positions.end(), {float(r * std::cos(a)), float(r * std::sin(a)), float(h * j / up)});
        }
    for (int j = 0; j < up; ++j)
        for (int i = 0; i < around; ++i) {
            const auto v = [&](int ii, int jj) { return std::uint32_t(jj * around + (ii % around)); };
            g.indices.insert(g.indices.end(), {v(i, j), v(i + 1, j), v(i + 1, j + 1), v(i, j), v(i + 1, j + 1), v(i, j + 1)});
        }
    return g;
}

double area(const sewing::Piece& p)
{
    double a = 0;
    for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
        const auto& A = p.points[p.indices[t]];
        const auto& B = p.points[p.indices[t + 1]];
        const auto& C = p.points[p.indices[t + 2]];
        a += 0.5 * ((B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]));
    }
    return a;
}

}  // namespace

TEST_CASE("Sewing: a flat square lies as it is", "[sewing]")
{
    Grid g;
    const int n = 10;
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i)
            g.positions.insert(g.positions.end(), {float(i * 4.0), -5.0f, float(j * 3.0)});  // standing, facing -Y
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            const auto v = [&](int ii, int jj) { return std::uint32_t(jj * (n + 1) + ii); };
            g.indices.insert(g.indices.end(), {v(i, j), v(i + 1, j + 1), v(i + 1, j), v(i, j), v(i, j + 1), v(i + 1, j + 1)});
        }
    const auto p = sewing::make(g.positions, g.indices, std::vector<bool>(g.indices.size() / 3, true), sewing::Split::None);
    REQUIRE(p.pieces.size() == 1);
    const auto& piece = p.pieces[0];
    CHECK(piece.name == "Piece");
    CHECK(piece.worst < 1e-3);
    CHECK_THAT(piece.width, WithinAbs(40.0, 0.05));   // up the figure stays up the page
    CHECK_THAT(piece.height, WithinAbs(30.0, 0.05));
    CHECK(area(piece) > 0);  // seen from outside, not mirrored
    REQUIRE(piece.outline.size() == 1);
    CHECK(p.problem.empty());
}

TEST_CASE("Sewing: a tube split at the sides unrolls into a front and a back", "[sewing]")
{
    const double r = 10, h = 30;
    const Grid g = tube(r, h, 72, 12);
    const auto p = sewing::make(g.positions, g.indices, std::vector<bool>(g.indices.size() / 3, true),
                                sewing::Split::FrontBack);
    REQUIRE(p.pieces.size() == 2);
    CHECK(p.pieces[0].name == "Front");
    CHECK(p.pieces[1].name == "Back");
    for (const auto& piece : p.pieces) {
        CHECK(piece.stretch < 0.01);
        CHECK_THAT(piece.height, WithinAbs(h, 0.3));
        // Half way round (the polygon's, a hair under pi r).
        CHECK_THAT(piece.width, WithinRel(std::numbers::pi * r, 0.01));
        CHECK_THAT(area(piece), WithinRel(std::numbers::pi * r * h, 0.01));
        REQUIRE(piece.outline.size() == 1);
    }

    // Left and right: the same, the other way.
    const auto lr = sewing::make(g.positions, g.indices, std::vector<bool>(g.indices.size() / 3, true),
                                 sewing::Split::LeftRight);
    REQUIRE(lr.pieces.size() == 2);
    CHECK(lr.pieces[0].name == "Right");
    CHECK(lr.pieces[1].name == "Left");

    // Not cut, it can't lie flat: said so.
    const auto whole = sewing::make(g.positions, g.indices, std::vector<bool>(g.indices.size() / 3, true),
                                    sewing::Split::None);
    CHECK_FALSE(whole.problem.empty());
}

TEST_CASE("Sewing: only the chosen triangles, two sleeves are two pieces", "[sewing]")
{
    Grid a = tube(5, 20, 48, 8);
    Grid b = tube(5, 20, 48, 8);
    const auto offset = std::uint32_t(a.positions.size() / 3);
    for (std::size_t i = 0; i < b.positions.size(); i += 3)
        b.positions[i] += 30.0f;
    a.positions.insert(a.positions.end(), b.positions.begin(), b.positions.end());
    for (auto v : b.indices)
        a.indices.push_back(v + offset);
    std::vector<bool> chosen(a.indices.size() / 3, true);
    const auto p = sewing::make(a.positions, a.indices, chosen, sewing::Split::FrontBack);
    REQUIRE(p.pieces.size() == 4);
    CHECK(p.pieces[0].name == "Front 1");
    CHECK(p.pieces[1].name == "Front 2");
    CHECK(p.pieces[3].name == "Back 2");
    CHECK_THROWS_AS(sewing::make(a.positions, a.indices, std::vector<bool>(chosen.size(), false), sewing::Split::None),
                    std::invalid_argument);
}

TEST_CASE("Sewing: from the paint on a shape", "[sewing]")
{
    Document doc;
    CommandStack stack{doc};
    Body cyl;
    cyl.id = doc.allocateId();
    cyl.name = "Arm";
    cyl.spec = CylinderSpec{6.0, 30.0};
    stack.execute(std::make_unique<AddBodyCommand>(cyl));
    // A band of red round the middle.
    std::vector<PaintDab> dabs;
    for (int z = 8; z <= 22; z += 2)
        for (int i = 0; i < 36; ++i) {
            const double a = 2 * std::numbers::pi * i / 36;
            dabs.push_back({{6 * std::cos(a), 6 * std::sin(a), double(z)}, 2.0, 0xCC2222, false});
        }
    Body painted;
    painted.id = doc.allocateId();
    painted.name = "Arm";
    painted.boolean = paintSpec(doc, cyl.id, dabs);
    stack.execute(std::make_unique<CombineCommand>(painted));
    const auto colours = sewing::paintedColours(doc, painted.id);
    REQUIRE(colours.size() == 1);
    const auto p = sewing::fromPaint(doc, painted.id, colours[0], sewing::Split::FrontBack);
    REQUIRE(p.pieces.size() == 2);
    for (const auto& piece : p.pieces) {
        CHECK_THAT(piece.width, WithinRel(std::numbers::pi * 6, 0.06));
        CHECK(piece.height > 13.0);
        CHECK(piece.height < 20.0);
        CHECK(piece.stretch < 0.03);
    }
    CHECK_THROWS_AS(sewing::fromPaint(doc, cyl.id, colours[0], sewing::Split::FrontBack), std::invalid_argument);
}

TEST_CASE("Sewing: a shirt painted on a T-posed body: a front and a back with sleeves", "[sewing]")
{
    Document doc;
    CommandStack stack{doc};
    const auto add = [&](const PrimitiveSpec& spec, double x, double y, double z, double turnY) {
        Body b;
        b.id = doc.allocateId();
        b.name = "Part";
        b.spec = spec;
        b.placement.x = x;
        b.placement.y = y;
        b.placement.z = z;
        b.placement.rotationY = turnY;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        return b.id;
    };
    const ObjectId body = add(CylinderSpec{8.0, 5.5, 34.0}, 0, 0, 0, 0);
    const ObjectId left = add(CylinderSpec{2.5, 20.0}, 15, 0, 16, 90);   // out to +X (turned about its middle)
    const ObjectId right = add(CylinderSpec{2.5, 20.0}, -15, 0, 16, -90); // out to -X
    Body figure;
    figure.id = doc.allocateId();
    figure.name = "Figure";
    figure.boolean = BooleanSpec{};
    figure.boolean->op = BooleanOp::Merge;
    figure.boolean->operands = {body, left, right};
    stack.execute(std::make_unique<CombineCommand>(figure));
    // The shirt: the body from z 14 up to just under the top, and the arms to the elbows.
    std::vector<PaintDab> dabs;
    for (double z = 14; z <= 32; z += 2)
        for (double x = -9; x <= 9; x += 2)
            for (double y = -7; y <= 7; y += 2)
                dabs.push_back({{x, y, z}, 1.6, 0x3355CC, false});
    for (double x = 8; x <= 17; x += 2)
        for (double y = -3; y <= 3; y += 2)
            for (double z = 23; z <= 29; z += 2) {
                dabs.push_back({{x, y, z}, 1.6, 0x3355CC, false});
                dabs.push_back({{-x, y, z}, 1.6, 0x3355CC, false});
            }
    Body painted;
    painted.id = doc.allocateId();
    painted.name = "Figure";
    painted.boolean = paintSpec(doc, figure.id, dabs);
    stack.execute(std::make_unique<CombineCommand>(painted));
    if (const char* folder = std::getenv("FORGE_DUMP"))
        file::save(doc, std::string(folder) + "/shirt.f3d", "test");
    const auto p = sewing::fromPaint(doc, painted.id, 0x3355CC, sewing::Split::FrontBack);
    REQUIRE(p.pieces.size() == 2);
    for (const auto& piece : p.pieces) {
        // Wide across the sleeves, short up and down.
        CHECK(piece.width > 35.0);
        CHECK(piece.height > 15.0);
        CHECK(piece.height < piece.width);
        // Two plain cylinders meet in a sharp corner under each arm, which can't lie flat;
        // the rest does.
        CHECK(piece.stretch < 0.12);
    }
}

TEST_CASE("Sewing: a painted seam line opens a tube, and no cloth goes missing", "[sewing]")
{
    const double r = 10, h = 30;
    const int around = 72, up = 12;
    const Grid g = tube(r, h, around, up);
    const std::size_t count = g.indices.size() / 3;
    // A seam line down one side: the triangles of one column (about 0.9 mm wide).
    const auto column = [&](std::size_t t) { return int((t / 2) % std::size_t(around)); };
    std::vector<bool> chosen(count), seams(count);
    for (std::size_t t = 0; t < count; ++t) {
        seams[t] = column(t) == 0;
        chosen[t] = !seams[t];
    }
    const auto one = sewing::make(g.positions, g.indices, chosen, sewing::Split::None, seams);
    REQUIRE(one.pieces.size() == 1);
    CHECK(one.problem.empty());
    const auto& piece = one.pieces[0];
    CHECK(piece.stretch < 0.02);
    CHECK_THAT(piece.height, WithinAbs(h, 0.5));
    // All the way round (the seam line's own cloth shared out, none lost).
    CHECK_THAT(area(piece), WithinRel(2 * std::numbers::pi * r * h, 0.02));
    CHECK_THAT(piece.width, WithinRel(2 * std::numbers::pi * r, 0.03));

    // Two seam lines, opposite: two halves.
    for (std::size_t t = 0; t < count; ++t) {
        seams[t] = column(t) == 0 || column(t) == around / 2;
        chosen[t] = !seams[t];
    }
    const auto two = sewing::make(g.positions, g.indices, chosen, sewing::Split::None, seams);
    REQUIRE(two.pieces.size() == 2);
    for (const auto& p : two.pieces)
        CHECK_THAT(area(p), WithinRel(std::numbers::pi * r * h, 0.03));

    // With a seam down the middle front and back too (the lines are at the sides): four.
    const auto four = sewing::make(g.positions, g.indices, chosen, sewing::Split::LeftRight, seams);
    CHECK(four.pieces.size() == 4);

    // Seam paint off on its own (not touching the cloth) changes nothing.
    std::vector<bool> stray(count, false);
    const auto plain = sewing::make(g.positions, g.indices, std::vector<bool>(count, true), sewing::Split::FrontBack, stray);
    CHECK(plain.pieces.size() == 2);
}
