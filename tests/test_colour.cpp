// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

using namespace forge;
namespace fs = std::filesystem;

namespace {

ObjectId addBox(CommandStack& stack, Document& doc, double x)
{
    Body b;
    b.id = doc.allocateId();
    b.name = doc.uniqueName("Box");
    b.spec = BoxSpec{10, 10, 10};
    b.placement.x = x;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

}  // namespace

TEST_CASE("Colours: chosen, inherited, undone and saved", "[colour]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = addBox(stack, doc, 0);
    const ObjectId b = addBox(stack, doc, 5);
    // Automatic colours, in turn.
    CHECK(doc.colourOf(a) == kDefaultColours[0]);
    CHECK(doc.colourOf(b) == kDefaultColours[1]);

    stack.execute(std::make_unique<SetColourCommand>(std::vector<ObjectId>{a, b}, 0xD7322Du));
    CHECK(doc.colourOf(a) == 0xD7322D);
    CHECK(doc.colourOf(b) == 0xD7322D);
    stack.execute(std::make_unique<SetColourCommand>(std::vector<ObjectId>{b}, 0x2B78D0u));
    stack.undo();
    CHECK(doc.colourOf(b) == 0xD7322D);
    stack.undo();
    CHECK_FALSE(doc.find(a)->colour.has_value());
    CHECK(doc.colourOf(b) == kDefaultColours[1]);
    stack.redo();

    // A merge shows its first part's colour, until it's given its own.
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Merge";
    merged.boolean = BooleanSpec{};
    merged.boolean->op = BooleanOp::Merge;
    merged.boolean->operands = {a, b};
    stack.execute(std::make_unique<CombineCommand>(merged));
    CHECK(doc.colourOf(merged.id) == 0xD7322D);
    stack.execute(std::make_unique<SetColourCommand>(std::vector<ObjectId>{merged.id}, 0x3FA458u));
    CHECK(doc.colourOf(merged.id) == 0x3FA458);
    CHECK(doc.colourOf(a) == 0xD7322D);  // the parts keep theirs

    // Colouring doesn't rebuild the shape.
    const auto before = doc.mesh(merged.id);
    stack.execute(std::make_unique<SetColourCommand>(std::vector<ObjectId>{merged.id}, 0x8A56C2u));
    CHECK(doc.mesh(merged.id) == before);

    // Saved and read back.
    const auto bodies = file::fromJson(file::toJson(doc, "test"));
    Document again;
    again.replaceAll(bodies);
    CHECK(again.colourOf(merged.id) == 0x8A56C2);
    CHECK(again.colourOf(a) == 0xD7322D);
    CHECK(again.find(b)->colour == std::optional<std::uint32_t>(0xD7322D));
}

TEST_CASE("Colours go out in 3MF files and come back in", "[colour][import]")
{
    const fs::path dir = fs::temp_directory_path() / ("forge3d-colour-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = addBox(stack, doc, 0);
    const ObjectId b = addBox(stack, doc, 20);
    const ObjectId c = addBox(stack, doc, 40);
    stack.execute(std::make_unique<SetColourCommand>(std::vector<ObjectId>{a, c}, 0xF2C230u));
    const fs::path mf = dir / "three.3mf";
    writeMeshes(exportMeshes(doc, {a, b, c}), mf, ExportFormat::ThreeMf);

    const auto parts = readMeshFile(mf);
    REQUIRE(parts.size() == 3);
    CHECK(parts[0].colour == std::optional<std::uint32_t>(0xF2C230));
    CHECK(parts[1].colour == std::optional<std::uint32_t>(doc.colourOf(b)));
    CHECK(parts[2].colour == std::optional<std::uint32_t>(0xF2C230));

    Document other;
    CommandStack otherStack(other);
    const ImportResult result = importFile(otherStack, other, mf);
    REQUIRE(result.bodies.size() == 3);
    CHECK(other.colourOf(result.bodies[0]) == 0xF2C230);

    // Files without colours: none given.
    const fs::path stl = dir / "one.stl";
    writeMeshes(exportMeshes(doc, {a}), stl, ExportFormat::Stl);
    CHECK_FALSE(readMeshFile(stl)[0].colour.has_value());

    // A damaged colour in a project file is refused.
    std::string json = file::toJson(doc, "test");
    const auto at = json.find("#F2C230");
    REQUIRE(at != std::string::npos);
    json.replace(at, 7, "#F2C23Z");
    CHECK_THROWS_AS(file::fromJson(json), file::FileError);
    fs::remove_all(dir);
}
