// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 12: Emboss a picture (logos traced to outlines; photos as reliefs).
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Picture.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

// A w x h picture, white, with a black square ring: outside `outer` pixels across,
// a white hole `inner` across, both centred.
std::shared_ptr<const PictureData> ring(int w, int h, int outer, int inner)
{
    auto p = std::make_shared<PictureData>();
    p->width = w;
    p->height = h;
    p->grey.assign(std::size_t(w) * h, 255);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int dx = std::abs(2 * x + 1 - w), dy = std::abs(2 * y + 1 - h);
            const bool inOuter = dx < outer && dy < outer;
            const bool inInner = dx < inner && dy < inner;
            if (inOuter && !inInner)
                p->grey[std::size_t(y) * w + x] = 0;
        }
    return p;
}

// Left half black, right half white, shading evenly from black to white.
std::shared_ptr<const PictureData> ramp(int w, int h)
{
    auto p = std::make_shared<PictureData>();
    p->width = w;
    p->height = h;
    p->grey.resize(std::size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            p->grey[std::size_t(y) * w + x] = std::uint8_t(std::lround(255.0 * x / (w - 1)));
    return p;
}

double areaOf(const std::vector<text::FlatPiece>& pieces)
{
    double a = 0.0;
    for (const auto& piece : pieces)
        for (std::size_t t = 0; t + 2 < piece.triangles.size(); t += 3) {
            const auto& p = piece.points[piece.triangles[t]];
            const auto& q = piece.points[piece.triangles[t + 1]];
            const auto& r = piece.points[piece.triangles[t + 2]];
            a += ((q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])) / 2.0;
        }
    return a;
}

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId embossed(CommandStack& stack, Document& doc, ObjectId target, Vec3 from, Vec3 dir, const EmbossPicture& pic)
{
    const auto spot = surfaceUnder(doc, target, from, dir, 2.0);
    REQUIRE(spot);
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Emboss");
    r.boolean = embossSpec(doc, target, *spot, pic);
    REQUIRE_FALSE(doc.previewMesh(*r.boolean)->empty());
    const ObjectId id = r.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(r)));
    return id;
}

double vol(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

}  // namespace

TEST_CASE("Pictures: grey from colours, shrunk to fit", "[picture]")
{
    // 2 x 1: a red pixel and a see-through one (white).
    const auto p = picture::fromRgba(2, 1, {255, 0, 0, 255, 0, 0, 0, 0});
    REQUIRE(p->width == 2);
    CHECK(p->grey[0] == 76);
    CHECK(p->grey[1] == 255);
    // A big one shrinks to 1000 on its longest side, keeping its shape.
    std::vector<std::uint8_t> big(std::size_t(3000) * 1500 * 4, 0);
    for (std::size_t i = 3; i < big.size(); i += 4)
        big[i] = 255;  // black, not see-through
    const auto shrunk = picture::fromRgba(3000, 1500, big);
    CHECK(shrunk->width == 1000);
    CHECK(shrunk->height == 500);
    CHECK(shrunk->grey[0] == 0);
}

TEST_CASE("Pictures: a logo traced to outlines", "[picture]")
{
    // A black square ring, 60 px across with a 20 px hole, in 100 x 80.
    const auto image = ring(100, 80, 60, 20);
    const auto loops = picture::outlines(*image, 128, false);
    REQUIRE(loops.size() == 2);  // the outside and the hole
    // Simplified: a square needs only its corners.
    CHECK(loops[0].size() <= 12);
    CHECK(loops[1].size() <= 12);
    // Inverted, the white is the shape: the frame round the ring and the hole's square.
    CHECK(picture::outlines(*image, 128, true).size() == 3);

    EmbossPicture pic;
    pic.image = image;
    pic.width = 50.0;  // 0.5 mm a pixel
    const auto pieces = picture::flatPieces(pic, 1.0);
    // The ring: 30 x 30 mm less the 10 x 10 hole (the outline runs between pixels).
    CHECK_THAT(areaOf(pieces), WithinRel(800.0, 0.03));
    CHECK_THAT(picture::heightOf(pic), WithinAbs(40.0, 1e-9));
    for (const auto& piece : pieces)
        for (std::size_t t = 0; t + 2 < piece.triangles.size(); t += 3)
            for (int e = 0; e < 3; ++e) {
                const auto& a = piece.points[piece.triangles[t + e]];
                const auto& b = piece.points[piece.triangles[t + (e + 1) % 3]];
                CHECK(std::hypot(a[0] - b[0], a[1] - b[1]) <= 1.0 + 1e-9);
            }

    // An all-white picture has nothing to emboss.
    auto white = std::make_shared<PictureData>();
    white->width = white->height = 10;
    white->grey.assign(100, 255);
    pic.image = white;
    CHECK_THROWS_WITH(picture::flatPieces(pic, 1.0), Catch::Matchers::ContainsSubstring("nothing to emboss"));
}

TEST_CASE("Pictures: a photo relief", "[picture]")
{
    EmbossPicture pic;
    pic.image = ramp(100, 50);
    pic.relief = true;
    pic.width = 40.0;
    const auto pieces = picture::flatPieces(pic, 1.0);
    REQUIRE(pieces.size() == 1);
    const auto& sheet = pieces.front();
    CHECK_THAT(areaOf(pieces), WithinRel(40.0 * 20.0, 1e-9));
    REQUIRE(sheet.heights.size() == sheet.points.size());
    // Black (left) stands highest, white (right) lowest; inverted the other way.
    double left = 0.0, right = 0.0;
    for (std::size_t i = 0; i < sheet.points.size(); ++i) {
        if (sheet.points[i][0] < -19.0)
            left = std::max(left, sheet.heights[i]);
        if (sheet.points[i][0] > 19.0)
            right = std::max(right, sheet.heights[i]);
    }
    CHECK(left > 0.95);
    CHECK(right < 0.05);
}

TEST_CASE("Emboss a picture on flat and curved faces", "[picture][emboss]")
{
    Document doc;
    CommandStack stack(doc);
    EmbossPicture pic;
    pic.name = "ring.png";
    pic.image = ring(100, 100, 60, 20);
    pic.width = 20.0;  // 0.2 mm a pixel: the ring 12 x 12 less 4 x 4 = 128 square mm
    pic.depth = 1.0;

    SECTION("a logo raised on a box, and engraved")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 10});
        const double before = vol(doc, box);
        const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, pic);
        CHECK_THAT(doc.worldBounds(e).max[2], WithinAbs(11.0, 1e-3));
        CHECK_THAT(vol(doc, e) - before, WithinRel(128.0, 0.04));
        CHECK(exportMeshes(doc, {e}).front().openEdges == 0);
        CHECK(history::describe(doc, e).find("ring.png") != std::string::npos);

        pic.raised = false;
        const Body changed = withEmboss(doc, e, embossSpot(doc, e), pic);
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(e), changed, "Change picture"));
        CHECK_THAT(before - vol(doc, e), WithinRel(128.0, 0.04));
        CHECK_THAT(doc.worldBounds(e).max[2], WithinAbs(10.0, 1e-3));
    }
    SECTION("a relief: half black to white across, so half its full depth on average")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 10});
        const double before = vol(doc, box);
        pic.image = ramp(100, 50);
        pic.relief = true;
        const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, pic);
        // 20 x 10 mm, from 1 mm high down to (almost) nothing: about 100 cubic mm.
        CHECK_THAT(vol(doc, e) - before, WithinRel(100.0, 0.05));
        CHECK_THAT(doc.worldBounds(e).max[2], WithinAbs(11.0, 0.02));
    }
    SECTION("round a cylinder, it follows the curve")
    {
        const ObjectId cyl = add(stack, doc, CylinderSpec{10, 30});
        const double before = vol(doc, cyl);
        const ObjectId e = embossed(stack, doc, cyl, {0, -50, 15}, {0, 1, 0}, pic);
        CHECK_THAT(vol(doc, e) - before, WithinRel(128.0 * (1 + 0.5 / 10), 0.06));
        // It stands 1 mm off the curve all round, not out flat like a sticker.
        CHECK(doc.worldBounds(e).min[1] > -11.05);
        CHECK(exportMeshes(doc, {e}).front().openEdges == 0);
    }
    SECTION("too wide to go round")
    {
        // The way round the box is 38 mm; the ring 48 mm.
        const ObjectId box = add(stack, doc, BoxSpec{15, 15, 4});
        pic.width = 80.0;
        const auto spot = surfaceUnder(doc, box, {0, 0, 50}, {0, 0, -1}, 2.0);
        REQUIRE(spot);
        CHECK_THROWS_WITH(doc.previewMesh(embossSpec(doc, box, *spot, pic)),
                          Catch::Matchers::ContainsSubstring("wider than the way round"));
    }
}

TEST_CASE("Emboss pictures save, load and scale", "[picture][emboss]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-picture-" + std::to_string(rd()));
    fs::create_directories(dir);
    Document doc;
    CommandStack stack(doc);
    EmbossPicture pic;
    pic.name = "photo.jpg";
    pic.image = ramp(64, 32);
    pic.relief = true;
    pic.invert = true;
    pic.threshold = 90;
    pic.width = 16.0;
    pic.angle = 30.0;
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, pic);

    file::save(doc, dir / "p.f3d", "test");
    Document loaded;
    loaded.replaceAll(file::load(dir / "p.f3d"));
    CHECK(*loaded.find(e) == *doc.find(e));
    CHECK_THAT(meshkernel::volume(loaded.meshSolid(e)), WithinRel(vol(doc, e), 1e-9));

    const auto after = scaledTree(doc, e, {2, 2, 2}, {0, 0, 0});
    std::vector<Body> before;
    for (const Body& x : after)
        before.push_back(*doc.find(x.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
    CHECK_THAT(doc.find(e)->boolean->picture->width, WithinRel(32.0, 1e-9));
    CHECK_THAT(doc.find(e)->boolean->picture->depth, WithinRel(2.0, 1e-9));
    std::error_code ec;
    fs::remove_all(dir, ec);
}
