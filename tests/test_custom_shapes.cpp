// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 45: the custom shape library.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/ShapeLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
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

ObjectId step(CommandStack& stack, Document& doc, BooleanSpec spec, const std::string& name)
{
    Body r;
    r.id = doc.allocateId();
    r.name = name;
    r.boolean = std::move(spec);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

// A new, empty folder for a test's library, removed afterwards.
struct Folder {
    fs::path path;
    Folder()
    {
        std::random_device rd;
        path = fs::temp_directory_path() / ("forge3d-library-" + std::to_string(rd()));
        fs::remove_all(path);
    }
    ~Folder()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

}  // namespace

TEST_CASE("Shape library: file names", "[library]")
{
    CHECK(library::fileNameFor("Ball joint 3mm") == "Ball joint 3mm.f3d");
    CHECK(library::fileNameFor("  3/4\" peg: hex?  ") == "3-4- peg- hex-.f3d");
    CHECK(library::fileNameFor("socket...") == "socket.f3d");
    CHECK(library::fileNameFor(std::string(200, 'x')).size() == 80 + 4);
    CHECK_THROWS_AS(library::fileNameFor("  ..  "), std::invalid_argument);
    CHECK_THROWS_AS(library::fileNameFor(""), std::invalid_argument);
}

TEST_CASE("Shape library: a shape comes back at its saved size, beside the others", "[library]")
{
    Folder lib;
    Document doc;
    CommandStack stack(doc);
    SphereSpec ball;
    ball.radiusX = ball.radiusY = ball.radiusZ = 1.5;  // a 3 mm ball
    const ObjectId id = add(stack, doc, ball, 50, 20, 7);
    CHECK(library::list(lib.path).empty());  // no folder yet
    const fs::path file = library::save(doc, {id}, lib.path, "Ball joint 3mm", "test");
    CHECK(file.filename() == "Ball joint 3mm.f3d");
    CHECK(doc.find(id)->name != "Ball joint 3mm");  // the shape in the model isn't renamed
    const auto entries = library::list(lib.path);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].name == "Ball joint 3mm");

    // Into a new model: in the middle of the plate, resting on it, the same size.
    Document other;
    CommandStack otherStack(other);
    const auto made = library::add(otherStack, other, entries[0].file);
    REQUIRE(made.size() == 1);
    const Body* b = other.find(made[0]);
    CHECK(b->name == "Ball joint 3mm");
    REQUIRE(std::holds_alternative<SphereSpec>(b->spec));
    CHECK(std::get<SphereSpec>(b->spec).radiusX == 1.5);
    const Bounds box = other.worldBounds(made[0]);
    CHECK_THAT(box.min[2], WithinAbs(0.0, 1e-4));
    CHECK_THAT(box.max[2], WithinAbs(3.0, 1e-3));
    CHECK_THAT(box.center()[0], WithinAbs(0.0, 1e-4));
    CHECK_THAT(box.center()[1], WithinAbs(0.0, 1e-4));
    CHECK(otherStack.undoLabel() == "Add Ball joint 3mm");

    // Again: a new name, beside the first, not on top of it.
    const auto again = library::add(otherStack, other, entries[0].file);
    CHECK(other.find(again[0])->name == "Ball joint 3mm 2");
    CHECK(other.worldBounds(again[0]).min[0] > box.max[0]);
    otherStack.undo();
    CHECK(other.find(again[0]) == nullptr);
    CHECK(other.find(made[0]) != nullptr);

    // Saving under the same name replaces it.
    library::save(doc, {id}, lib.path, "Ball joint 3mm", "test");
    CHECK(library::list(lib.path).size() == 1);
}

TEST_CASE("Shape library: shapes keep their history and how they sit together", "[library]")
{
    Folder lib;
    Document doc;
    CommandStack stack(doc);
    // A socket: a box with a hole, made with Subtract (so it can still be changed).
    const ObjectId block = add(stack, doc, BoxSpec{10, 10, 6}, 30, 0, 0);
    const ObjectId hole = add(stack, doc, CylinderSpec{1.6, 10}, 30, 0, 2);
    BooleanSpec cut;
    cut.op = BooleanOp::Subtract;
    cut.operands = {block, hole};
    const ObjectId socket = step(stack, doc, cut, "Socket");
    const ObjectId peg = add(stack, doc, CylinderSpec{1.5, 8}, 45, 0, 0);
    const double socketVolume = volumeOf(doc, socket);
    const double gap = doc.worldBounds(peg).min[0] - doc.worldBounds(socket).max[0];

    // A part can't be saved on its own.
    CHECK_THROWS_AS(library::save(doc, {hole}, lib.path, "Hole", "test"), std::invalid_argument);
    CHECK_THROWS_AS(library::save(doc, {}, lib.path, "Nothing", "test"), std::invalid_argument);

    const fs::path file = library::save(doc, {socket, peg}, lib.path, "Peg and socket", "test");
    Document other;
    CommandStack otherStack(other);
    add(otherStack, other, BoxSpec{20, 20, 20});  // something already there
    const auto made = library::add(otherStack, other, file);
    REQUIRE(made.size() == 2);
    const Body* s = other.find(made[0]);
    CHECK(s->name == "Socket");  // two shapes keep their own names
    REQUIRE(s->isBoolean());
    CHECK(s->boolean->operands.size() == 2);
    CHECK_THAT(volumeOf(other, made[0]), WithinRel(socketVolume, 1e-6));
    // Still apart by the same gap, and clear of the box already there.
    CHECK_THAT(other.worldBounds(made[1]).min[0] - other.worldBounds(made[0]).max[0], WithinAbs(gap, 1e-4));
    CHECK(other.worldBounds(made[0]).min[0] > 10.0);
    // One Undo takes them both (and their parts) away.
    const std::size_t count = other.bodies().size();
    otherStack.undo();
    CHECK(other.bodies().size() == count - 4);
}

TEST_CASE("Shape library: worked-out results come with the shape", "[library]")
{
    Folder lib;
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{12, 12, 12}, 40, 15, 3);
    BooleanSpec s;
    s.op = BooleanOp::Smooth;
    s.operands = {box};
    s.smooth = SmoothFinish{3};
    const ObjectId smooth = step(stack, doc, s, "Rounded");
    const double v = volumeOf(doc, smooth);  // worked out here
    const fs::path file = library::save(doc, {smooth}, lib.path, "Rounded block", "test");

    Document other;
    CommandStack otherStack(other);
    const auto made = library::add(otherStack, other, file);
    REQUIRE(made.size() == 1);
    CHECK(other.cachedSolid(made[0]).has_value());  // not worked out again
    CHECK_THAT(volumeOf(other, made[0]), WithinRel(v, 1e-6));
    // Where it sits is the new shape's own, not the one it was saved from.
    const Bounds placed = other.worldBounds(made[0]);
    CHECK_THAT(placed.center()[0], WithinAbs(0.0, 1e-3));
    CHECK_THAT(placed.min[2], WithinAbs(0.0, 1e-3));
}

TEST_CASE("Shape library: rename", "[library]")
{
    Folder lib;
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{5, 5, 5});
    const fs::path first = library::save(doc, {a}, lib.path, "Alpha", "test");
    library::save(doc, {a}, lib.path, "Beta", "test");
    CHECK_THROWS_AS(library::rename(first, "Beta"), std::runtime_error);
    const fs::path renamed = library::rename(first, "Gamma");
    CHECK(renamed.filename() == "Gamma.f3d");
    const auto entries = library::list(lib.path);
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].name == "Beta");
    CHECK(entries[1].name == "Gamma");
    // Added, it's called what it's called now.
    const auto made = library::add(stack, doc, renamed);
    CHECK(doc.find(made[0])->name == "Gamma");
}
