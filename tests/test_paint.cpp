// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 66: painting colours onto one shape.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Paint.h"
#include "forge/core/Scale.h"

#include <miniz.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace forge;
using Catch::Matchers::WithinAbs;
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

ObjectId paintOn(CommandStack& stack, Document& doc, ObjectId target, const std::vector<PaintDab>& dabs)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Painted");
    r.boolean = paintSpec(doc, target, dabs);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

std::size_t countOf(const std::string& text, const std::string& what)
{
    std::size_t n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size()))
        ++n;
    return n;
}

constexpr std::uint32_t kRed = 0xD02020, kBlue = 0x2040D0, kSkin = 0xF1C27D;

}  // namespace

TEST_CASE("Paint: the last dab over a point decides its colour", "[paint]")
{
    const std::vector<PaintDab> dabs{
        {{0, 0, 0}, 100.0, kSkin, false},  // Fill All
        {{0, 0, 0}, 2.0, kRed, false},
        {{3, 0, 0}, 2.0, kBlue, false},    // overlaps the red one at x = 1..2
        {{10, 0, 0}, 2.0, 0, true},        // rubbed out: the shape's own colour
    };
    const paint::Field field(dabs);
    CHECK(field.colourAt({0, 0, 0}) == kRed);
    CHECK(field.colourAt({1.5, 0, 0}) == kBlue);
    CHECK(field.colourAt({3, 1, 0}) == kBlue);
    CHECK(field.colourAt({20, 20, 0}) == kSkin);
    CHECK(field.colourAt({10, 0, 0}) == paint::kOwnColour);
    CHECK(field.colourAt({500, 0, 0}) == paint::kOwnColour);
    CHECK(paint::colours(dabs) == std::vector<std::uint32_t>{kSkin, kRed, kBlue});
    CHECK(paint::describe(PaintFinish{dabs}) == "3 colours");
    CHECK(paint::describe(PaintFinish{{{{0, 0, 0}, 1.0, 0, true}}}) == "rubbed out");

    // Where one colour shows, as a mask: its dabs paint, the others rub out.
    const auto mask = paint::maskFor(dabs, kRed);
    REQUIRE(mask.size() == 3);  // the fill before the red dab changes nothing
    CHECK_FALSE(mask[0].erase);
    CHECK(mask[1].erase);
    CHECK(mask[2].erase);

    PaintFinish bad{{{{0, 0, 0}, 0.0, kRed, false}}};
    CHECK_THROWS_AS(paint::check(bad), std::invalid_argument);
}

TEST_CASE("Paint: a step that colours the shape, kept and moved with it", "[paint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, "Box");
    // The top half red (on the plate, the box stands from z 0 to 20), a blue spot on the front.
    const std::vector<PaintDab> dabs{{{0, 0, 30}, 15.0, kRed, false}, {{0, -10, 5}, 3.0, kBlue, false}};
    const ObjectId painted = paintOn(stack, doc, box, dabs);
    CHECK(history::describe(doc, painted) == "2 colours");

    // The shape is the same, divided finer so the paint's edges show.
    CHECK_THAT(meshkernel::volume(doc.meshSolid(painted)), WithinRel(8000.0, 1e-6));
    CHECK(meshkernel::triangleCount(doc.meshSolid(painted)) > 30000);

    const auto objectDabs = paint::objectPaint(doc, painted);
    REQUIRE(objectDabs.size() == 2);
    const auto mesh = doc.mesh(painted);
    const auto colours = paint::triangleColours(mesh->positions, mesh->indices, paint::Field(objectDabs));
    const auto count = [&](std::uint32_t c) { return std::count(colours.begin(), colours.end(), c); };
    CHECK(count(kRed) > 0);
    CHECK(count(kBlue) > 0);
    CHECK(count(paint::kOwnColour) > count(kBlue));

    // Moved and turned on the plate, the paint stays on the shape.
    Body moved = *doc.find(painted);
    moved.placement.x += 50;
    moved.placement.rotationZ = 90;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(painted), moved, "Move"));
    const auto after = paint::objectPaint(doc, painted);
    REQUIRE(after.size() == 2);
    const auto mesh2 = doc.mesh(painted);
    CHECK(paint::triangleColours(mesh2->positions, mesh2->indices, paint::Field(after)) == colours);

    // Exported to 3MF: each triangle's colour, standard and for the slicers.
    const auto meshes = exportMeshes(doc, {painted});
    REQUIRE(meshes.size() == 1);
    REQUIRE(meshes[0].triangleColours.size() == meshes[0].triangleCount());
    const auto dir = std::filesystem::temp_directory_path() / "forge_paint_export";
    std::filesystem::create_directories(dir);
    const auto file = dir / "painted.3mf";
    writeMeshes(meshes, file, ExportFormat::ThreeMf);
    std::ifstream in(file, std::ios::binary);
    const std::string zipBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove_all(dir);
    mz_zip_archive zip{};
    REQUIRE(mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0));
    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, "3D/3dmodel.model", &size, 0);
    REQUIRE(data);
    const std::string model(static_cast<const char*>(data), size);
    mz_free(data);
    mz_zip_reader_end(&zip);
    CHECK(countOf(model, "<base ") == 3);  // the box's own colour, red, blue
    // Every painted triangle refers to its colour (filaments 2 and 3: red and blue).
    const auto& tc = meshes[0].triangleColours;
    CHECK(countOf(model, " p1=") == std::size_t(tc.size() - std::count(tc.begin(), tc.end(), paint::kOwnColour)));
    CHECK(countOf(model, "paint_color=\"8\"") > 0);
    CHECK(countOf(model, "paint_color=\"0C\"") > 0);
    CHECK(countOf(model, "paint_color=") == countOf(model, " p1="));
    CHECK(model.find("xmlns:slic3rpe=") != std::string::npos);
    CHECK(countOf(model, "slic3rpe:mmu_segmentation=") == countOf(model, "paint_color="));
}

TEST_CASE("Paint: saved, rescaled, and split along a colour", "[paint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = add(stack, doc, SphereSpec{10}, "Ball");
    // The ball's top cap red (the sphere sits on the plate: centre at z 10).
    const std::vector<PaintDab> dabs{{{0, 0, 20}, 6.0, kRed, false}, {{0, 0, 0}, 4.0, kBlue, false}};
    const ObjectId painted = paintOn(stack, doc, ball, dabs);

    const auto path = std::filesystem::temp_directory_path() / "forge_paint_test.f3d";
    file::save(doc, path, "test");
    const auto back = file::load(path);
    std::filesystem::remove(path);
    const auto found = std::find_if(back.begin(), back.end(), [&](const Body& b) { return b.id == painted; });
    REQUIRE(found != back.end());
    REQUIRE(found->boolean->paint);
    CHECK(*found->boolean->paint == *doc.find(painted)->boolean->paint);

    // Split along the red: the cap comes off.
    const auto mask = paint::maskFor(paintInPlate(doc, painted), kRed);
    const auto pieces = splitByMask(stack, doc, painted, mask, SplitKeep::Both);
    REQUIRE(pieces.size() == 2);
    const double whole = meshkernel::volume(doc.meshSolid(ball));
    const double cap = meshkernel::volume(doc.meshSolid(pieces[0]));
    CHECK(cap > 0.0);
    CHECK(cap < 0.3 * whole);
    stack.undo();

    // Rescaled: the paint grows with the ball.
    auto after = rescaledDesign(doc, {painted}, 2.0, {0, 0, 0}, ScaleOptions{});
    std::vector<Body> old;
    for (const Body& b : after)
        old.push_back(*doc.find(b.id));
    stack.execute(std::make_unique<EditBodiesCommand>(old, after, "Rescale"));
    const PaintFinish& grown = *doc.find(painted)->boolean->paint;
    CHECK_THAT(grown.dabs[0].radius, WithinAbs(12.0, 1e-9));
    CHECK_THAT(grown.dabs[0].centre[2], WithinAbs(40.0, 1e-9));
}
