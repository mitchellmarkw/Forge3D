// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 5d: the Text shape and Emboss.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"
#include "forge/core/Text.h"

#include "forge/core/Transform.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId embossed(CommandStack& stack, Document& doc, ObjectId target, Vec3 rayFrom, Vec3 rayDir, EmbossText words)
{
    const auto spot = surfaceUnder(doc, target, rayFrom, rayDir, words.letterHeight / 2);
    REQUIRE(spot);
    Body r = embossBody(doc, target, *spot, words);
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

TEST_CASE("Fonts are found", "[text]")
{
    const auto fonts = text::availableFonts();
    REQUIRE_FALSE(fonts.empty());
    const std::string def = text::defaultFont();
    CHECK(std::find(fonts.begin(), fonts.end(), def) != fonts.end());
    CHECK(text::resolvedFont("No Such Font 123") == def);
    CHECK(text::capitalRatio("", false, false) > 0.5);
    CHECK(text::capitalRatio("", false, false) < 0.9);
}

TEST_CASE("Text shapes", "[text]")
{
    Document doc;
    CommandStack stack(doc);
    TextSpec t;
    t.text = "AB";
    t = text::sizedForLetters(t, 10.0);
    t.height = 2.0;
    CHECK_THAT(text::letterHeightOf(t), WithinRel(10.0, 1e-6));
    const ObjectId id = add(stack, doc, t);
    const Bounds b = doc.worldBounds(id);
    CHECK_THAT(b.size()[0], WithinAbs(t.width, 0.02));
    CHECK_THAT(b.size()[1], WithinAbs(t.depth, 0.02));
    CHECK_THAT(b.size()[2], WithinAbs(2.0, 1e-6));
    CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-6));
    CHECK_THAT(b.center()[0], WithinAbs(0.0, 0.02));

    SECTION("more words make it longer, the letters stay the same size")
    {
        const TextSpec longer = text::withText(t, "ABC", t.font, false, false);
        CHECK(longer.width > t.width * 1.3);
        CHECK_THAT(longer.depth, WithinRel(t.depth, 0.05));
        CHECK_THAT(text::letterHeightOf(longer), WithinRel(10.0, 1e-6));
    }
    SECTION("stretched text keeps its stretch")
    {
        TextSpec wide = t;
        wide.width *= 2;
        const ObjectId w = add(stack, doc, wide, 50);
        CHECK_THAT(doc.worldBounds(w).size()[0], WithinAbs(wide.width, 0.05));
        const TextSpec next = text::withText(wide, "ABAB", wide.font, false, false);
        CHECK(next.width > wide.width * 1.8);
    }
    SECTION("no words is refused")
    {
        CHECK_THROWS_AS(text::withText(t, "   ", t.font, false, false), std::invalid_argument);
        TextSpec empty = t;
        empty.text = "";
        CHECK_THROWS(text::makeText(empty));
    }
    SECTION("bold, italic and two lines")
    {
        const TextSpec bold = text::withText(t, "AB", "", true, true);
        CHECK_NOTHROW(text::makeText(bold));
        const TextSpec lines = text::withText(t, "AB\nCD", "", false, false);
        CHECK(lines.depth > t.depth * 1.8);
    }
    SECTION("stacked letters run top to bottom")
    {
        const TextSpec down = text::withText(t, "ABCD", t.font, false, false, true);
        CHECK(down.vertical);
        CHECK(down.depth > 3.5 * down.width);  // four letters tall, one wide
        CHECK_THAT(text::letterHeightOf(down), WithinRel(10.0, 0.05));
        CHECK(text::stacked("AB C") == "A\nB\n \nC");
        CHECK(text::stacked("\xC3\xA9t\xC3\xA9") == "\xC3\xA9\nt\n\xC3\xA9");  // é t é: letters kept whole
        const ObjectId id2 = add(stack, doc, down, 80);
        CHECK_THAT(doc.worldBounds(id2).size()[1], WithinAbs(down.depth, 0.05));
    }
    SECTION("exports watertight")
    {
        const ExportMesh m = exportMeshes(doc, {id}).front();
        CHECK(m.openEdges == 0);
        CHECK(m.volume() > 0.0);
    }
}

TEST_CASE("Emboss on flat and curved faces", "[emboss]")
{
    Document doc;
    CommandStack stack(doc);
    EmbossText words;
    words.text = "HI";
    words.letterHeight = 8.0;
    words.depth = 1.0;

    SECTION("raised on top of a box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 10});
        const double before = vol(doc, box);
        const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, words);
        CHECK(doc.usesMeshEngine(e));
        const Bounds b = doc.worldBounds(e);
        CHECK_THAT(b.max[2], WithinAbs(11.0, 1e-3));
        CHECK_THAT(b.size()[0], WithinAbs(40.0, 1e-3));
        const double added = vol(doc, e) - before;
        CHECK(added > 8.0 * 8.0 * 0.15);  // letters cover a good part of their 8 x ~11 block, 1 deep
        CHECK(added < 8.0 * 20.0);
        const ExportMesh m = exportMeshes(doc, {e}).front();
        CHECK(m.openEdges == 0);
    }
    SECTION("engraved into the front of a box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 20});
        const double before = vol(doc, box);
        words.raised = false;
        const ObjectId e = embossed(stack, doc, box, {0, -50, 10}, {0, 1, 0}, words);
        const Bounds b = doc.worldBounds(e);
        CHECK_THAT(b.min[1], WithinAbs(-15.0, 1e-3));  // nothing sticks out
        const double removed = before - vol(doc, e);
        CHECK(removed > 8.0 * 8.0 * 0.15);
        CHECK(removed < 8.0 * 20.0);
        // Undo gives the box back.
        stack.undo();
        CHECK(doc.isTopLevel(box));
    }
    SECTION("raised round a cylinder follows the curve")
    {
        const ObjectId cyl = add(stack, doc, CylinderSpec{10, 30});
        const ObjectId e = embossed(stack, doc, cyl, {0, -50, 15}, {0, 1, 0}, words);
        const Bounds b = doc.worldBounds(e);
        CHECK_THAT(b.min[1], WithinAbs(-11.0, 0.05));  // 1 mm out from the curved side, no more
        CHECK(b.min[0] > -10.5);                         // letters bend with it, not sticking out flat
    }
    SECTION("long words wrap round a cylinder")
    {
        const ObjectId cyl = add(stack, doc, CylinderSpec{10, 30});
        words.text = "WRAPPED";  // about 45 mm long: well over half way round
        words.letterHeight = 6.0;
        const ObjectId e = embossed(stack, doc, cyl, {0, -50, 15}, {0, 1, 0}, words);
        const Bounds b = doc.worldBounds(e);
        // Round the sides and onto the back, never more than 1 mm out from the surface.
        CHECK_THAT(b.min[0], WithinAbs(-11.0, 0.1));
        CHECK_THAT(b.max[0], WithinAbs(11.0, 0.1));
        CHECK(b.max[1] < 11.0);
        CHECK(b.max[1] > 0.0);
        CHECK(exportMeshes(doc, {e}).front().openEdges == 0);
    }
    SECTION("stacked words on the side of a box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{30, 30, 60});
        words.text = "ABC";
        words.vertical = true;
        const ObjectId e = embossed(stack, doc, box, {0, -50, 30}, {0, 1, 0}, words);
        const Bounds b = doc.worldBounds(e);
        CHECK_THAT(b.min[1], WithinAbs(-16.0, 1e-3));
        // The letters stand in a column: taller than wide. Measure the words alone.
        std::vector<float> pos;
        std::vector<std::uint32_t> idx;
        meshkernel::triangles(doc.meshSolid(box), pos, idx);
        const auto spot = surfaceUnder(doc, box, {0, -50, 30}, {0, 1, 0}, 4);
        const MeshSolid w = emboss::letters(pos, idx, spot->point, spot->normal, words);
        const Bounds wb = meshkernel::bounds(w, Transform{});
        CHECK(wb.size()[2] > 2.5 * wb.size()[0]);
    }
    SECTION("letters keep their size on a ball")
    {
        // Each letter "I" of a long row should take the same room wherever it is.
        const ObjectId ball = add(stack, doc, SphereSpec{10});
        words.text = "IIIIIIIIIII";
        words.letterHeight = 4.0;
        std::vector<float> pos;
        std::vector<std::uint32_t> idx;
        meshkernel::triangles(doc.meshSolid(ball), pos, idx);
        const auto spot = surfaceUnder(doc, ball, {0, -50, 10}, {0, 1, 0}, 2);
        REQUIRE(spot);
        const double onBall = meshkernel::volume(emboss::letters(pos, idx, spot->point, spot->normal, words));
        const ObjectId box = add(stack, doc, BoxSpec{80, 80, 10}, 100);
        std::vector<float> pos2;
        std::vector<std::uint32_t> idx2;
        meshkernel::triangles(meshkernel::transformed(doc.meshSolid(box), doc.worldTransform(box)), pos2, idx2);
        const double onFlat =
            meshkernel::volume(emboss::letters(pos2, idx2, {100, 0, 10}, {0, 0, 1}, words));
        CHECK_THAT(onBall, WithinRel(onFlat, 0.15));  // the same letters, just bent
    }
    SECTION("raised on a ball")
    {
        const ObjectId ball = add(stack, doc, SphereSpec{15});
        const ObjectId e = embossed(stack, doc, ball, {0, -50, 15}, {0, 1, 0}, words);
        const Bounds b = doc.worldBounds(e);
        CHECK_THAT(b.min[1], WithinAbs(-16.0, 0.1));
        CHECK(vol(doc, e) > vol(doc, ball));
    }
    SECTION("text too big for the shape")
    {
        const ObjectId box = add(stack, doc, BoxSpec{10, 10, 10});
        words.text = "MUCH TOO LONG";
        const auto spot = surfaceUnder(doc, box, {0, 0, 50}, {0, 0, -1}, 4);
        REQUIRE(spot);
        Body r = embossBody(doc, box, *spot, words);
        CHECK_THROWS_AS(doc.previewMesh(*r.boolean), GeometryError);
    }
    SECTION("turned, then new words and a new spot")
    {
        const ObjectId box = add(stack, doc, BoxSpec{60, 60, 10});
        words.angle = 90;
        const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, words);
        const auto spot = embossSpot(doc, e);
        CHECK_THAT(spot.point[2], WithinAbs(10.0, 1e-6));
        CHECK_THAT(spot.normal[2], WithinAbs(1.0, 1e-9));
        EmbossText other = words;
        other.text = "HELLO";
        other.angle = 0;
        Body after = withEmboss(doc, e, {{10, 10, 10}, {0, 0, 1}}, other);
        const Body before = *doc.find(e);
        stack.execute(std::make_unique<EditBodyCommand>(before, after, "Edit emboss"));
        CHECK(doc.find(e)->boolean->emboss->text == "HELLO");
        CHECK_THAT(embossSpot(doc, e).point[0], WithinAbs(10.0, 1e-9));
    }
}

TEST_CASE("Emboss saves, loads and scales", "[emboss]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-emboss-" + std::to_string(rd()));
    fs::create_directories(dir);
    Document doc;
    CommandStack stack(doc);
    EmbossText words;
    words.text = "Ok";
    words.font = text::defaultFont();
    words.italic = true;
    words.vertical = true;
    words.angle = 15;
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 10});
    const ObjectId e = embossed(stack, doc, box, {0, 0, 50}, {0, 0, -1}, words);
    TextSpec t;
    t.text = "Name";
    t.bold = true;
    t.vertical = true;
    const ObjectId label = add(stack, doc, text::sizedForLetters(t, 6), 60);

    file::save(doc, dir / "e.f3d", "test");
    Document loaded;
    loaded.replaceAll(file::load(dir / "e.f3d"));
    CHECK(*loaded.find(e) == *doc.find(e));
    CHECK(*loaded.find(label) == *doc.find(label));
    CHECK_THAT(meshkernel::volume(loaded.meshSolid(e)), WithinRel(vol(doc, e), 1e-9));

    // Twice the size: the words grow too.
    const auto after = scaledTree(doc, e, {2, 2, 2}, {0, 0, 0});
    std::vector<Body> before;
    for (const Body& x : after)
        before.push_back(*doc.find(x.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
    CHECK_THAT(doc.find(e)->boolean->emboss->letterHeight, WithinRel(16.0, 1e-9));
    CHECK_THAT(doc.worldBounds(e).max[2], WithinAbs(22.0, 1e-3));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("Centering words on a face", "[emboss][center]")
{
    Document doc;
    CommandStack stack(doc);
    EmbossText words;
    words.letterHeight = 4.0;
    using emboss::Center;

    SECTION("the front of a box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 20});
        const emboss::SurfacePoint spot{{7, -15, 4}, {0, -1, 0}};
        const auto both = centeredSpot(doc, box, spot, words, Center::Both);
        REQUIRE(both);
        CHECK_THAT(both->point[0], WithinAbs(0.0, 1e-6));
        CHECK_THAT(both->point[1], WithinAbs(-15.0, 1e-6));
        CHECK_THAT(both->point[2], WithinAbs(10.0, 1e-6));
        CHECK_THAT(both->normal[1], WithinAbs(-1.0, 1e-9));
        const auto upDown = centeredSpot(doc, box, spot, words, Center::TopBottom);
        REQUIRE(upDown);
        CHECK_THAT(upDown->point[0], WithinAbs(7.0, 1e-6));
        CHECK_THAT(upDown->point[2], WithinAbs(10.0, 1e-6));
        const auto across = centeredSpot(doc, box, spot, words, Center::LeftRight);
        REQUIRE(across);
        CHECK_THAT(across->point[0], WithinAbs(0.0, 1e-6));
        CHECK_THAT(across->point[2], WithinAbs(4.0, 1e-6));
    }
    SECTION("the top of a box, words turned a quarter")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 30, 20});
        words.angle = 90;  // reading along +Y: "across" is now Y
        const emboss::SurfacePoint spot{{5, 3, 20}, {0, 0, 1}};
        const auto across = centeredSpot(doc, box, spot, words, Center::LeftRight);
        REQUIRE(across);
        CHECK_THAT(across->point[0], WithinAbs(5.0, 1e-6));
        CHECK_THAT(across->point[1], WithinAbs(0.0, 1e-6));
    }
    SECTION("up and down the side of a cylinder")
    {
        const ObjectId cyl = add(stack, doc, CylinderSpec{10, 30}, 50);
        const emboss::SurfacePoint spot{{50, -10, 5}, {0, -1, 0}};
        const auto upDown = centeredSpot(doc, cyl, spot, words, Center::TopBottom);
        REQUIRE(upDown);
        CHECK_THAT(upDown->point[2], WithinAbs(15.0, 1e-3));
        CHECK_THAT(upDown->point[1], WithinAbs(-10.0, 0.05));
    }
}
