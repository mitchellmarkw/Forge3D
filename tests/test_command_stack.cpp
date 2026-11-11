// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>

using namespace forge;

namespace {

Body newBox(Document& doc, double size = 20)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{size, size, size};
    b.name = doc.uniqueName("Box");
    return b;
}

ObjectId add(CommandStack& stack, Document& doc, double size = 20)
{
    Body b = newBox(doc, size);
    const ObjectId id = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
    return id;
}

// A command that always fails, to check the stack's error handling.
class FailingCommand final : public Command {
public:
    std::string label() const override { return "Fail"; }
    void redo(Document&) override { throw std::runtime_error("nope"); }
    void undo(Document&) override {}
};

}  // namespace

TEST_CASE("add, undo, redo", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    CHECK_FALSE(stack.canUndo());
    CHECK(stack.isClean());

    const ObjectId id = add(stack, doc);
    CHECK(doc.bodyCount() == 1);
    CHECK(stack.undoLabel() == "Add Box");
    CHECK_FALSE(stack.isClean());

    stack.undo();
    CHECK(doc.empty());
    CHECK(stack.redoLabel() == "Add Box");
    CHECK(stack.isClean());

    stack.redo();
    REQUIRE(doc.bodyCount() == 1);
    CHECK(doc.bodies()[0].id == id);  // same id comes back
}

TEST_CASE("delete restores the body at its old position", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc);
    const ObjectId b = add(stack, doc);
    const ObjectId c = add(stack, doc);
    const Body before = *doc.find(b);

    stack.execute(std::make_unique<RemoveBodyCommand>(b));
    CHECK(stack.undoLabel() == "Delete Box 2");
    CHECK(doc.bodyCount() == 2);

    stack.undo();
    REQUIRE(doc.bodyCount() == 3);
    CHECK(doc.bodies()[0].id == a);
    CHECK(doc.bodies()[1].id == b);
    CHECK(doc.bodies()[2].id == c);
    CHECK(*doc.find(b) == before);
}

TEST_CASE("a new edit discards the redo history", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    add(stack, doc);
    add(stack, doc);
    stack.undo();
    CHECK(stack.canRedo());

    add(stack, doc);
    CHECK_FALSE(stack.canRedo());
    CHECK(stack.count() == 2);
}

TEST_CASE("moves of the same body merge into one undo step", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = add(stack, doc);

    for (int i = 1; i <= 10; ++i)
        stack.execute(std::make_unique<SetPlacementCommand>(id, Placement{double(i), 0, 0, 0}));
    CHECK(stack.count() == 2);  // Add + one Move
    CHECK(doc.find(id)->placement.x == 10.0);

    stack.undo();
    CHECK(doc.find(id)->placement.x == 0.0);  // straight back to the start
    stack.redo();
    CHECK(doc.find(id)->placement.x == 10.0);
}

TEST_CASE("moves of different bodies do not merge", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc);
    const ObjectId b = add(stack, doc);
    stack.execute(std::make_unique<SetPlacementCommand>(a, Placement{1, 0, 0, 0}));
    stack.execute(std::make_unique<SetPlacementCommand>(b, Placement{2, 0, 0, 0}));
    CHECK(stack.count() == 4);
}

TEST_CASE("a move does not merge into the saved state", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = add(stack, doc);
    stack.execute(std::make_unique<SetPlacementCommand>(id, Placement{5, 0, 0, 0}));
    stack.setClean();
    stack.execute(std::make_unique<SetPlacementCommand>(id, Placement{9, 0, 0, 0}));
    CHECK(stack.count() == 3);
    stack.undo();
    CHECK(stack.isClean());
    CHECK(doc.find(id)->placement.x == 5.0);
}

TEST_CASE("macros undo and redo as one step", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc);
    const ObjectId b = add(stack, doc);
    const ObjectId c = add(stack, doc);

    int notifications = 0;
    stack.setListener([&] { ++notifications; });

    stack.beginMacro("Clear Plate");
    for (ObjectId id : {a, b, c})
        stack.execute(std::make_unique<RemoveBodyCommand>(id));
    CHECK(notifications == 0);  // quiet until the macro closes
    stack.endMacro();
    CHECK(notifications == 1);

    CHECK(doc.empty());
    CHECK(stack.count() == 4);
    CHECK(stack.undoLabel() == "Clear Plate");

    stack.undo();
    REQUIRE(doc.bodyCount() == 3);
    CHECK(doc.bodies()[0].id == a);
    CHECK(doc.bodies()[2].id == c);

    stack.redo();
    CHECK(doc.empty());
}

TEST_CASE("an empty macro records nothing", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    stack.beginMacro("Nothing");
    stack.endMacro();
    CHECK(stack.count() == 0);
    CHECK_THROWS_AS(stack.endMacro(), std::logic_error);
}

TEST_CASE("a failing command is not recorded", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    add(stack, doc);
    CHECK_THROWS(stack.execute(std::make_unique<FailingCommand>()));
    CHECK(stack.count() == 1);
    CHECK(stack.undoLabel() == "Add Box");

    CHECK_THROWS(stack.execute(std::make_unique<RemoveBodyCommand>(12345)));
    CHECK(stack.count() == 1);
}

TEST_CASE("an invalid resize is rejected and the size can be undone", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = add(stack, doc, 20);

    CHECK_THROWS(stack.execute(std::make_unique<SetSpecCommand>(id, BoxSpec{0, 1, 1})));
    CHECK(stack.count() == 1);

    stack.execute(std::make_unique<SetSpecCommand>(id, BoxSpec{30, 40, 50}));
    CHECK(stack.undoLabel() == "Resize Box");
    stack.undo();
    CHECK(std::get<BoxSpec>(doc.find(id)->spec) == BoxSpec{20, 20, 20});
}

TEST_CASE("rename can be undone", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = add(stack, doc);
    stack.execute(std::make_unique<RenameBodyCommand>(id, "Lid"));
    CHECK(doc.find(id)->name == "Lid");
    stack.undo();
    CHECK(doc.find(id)->name == "Box 1");
}

TEST_CASE("the history is capped at the limit", "[commands]")
{
    Document doc;
    CommandStack stack(doc, 5);
    for (int i = 0; i < 8; ++i)
        add(stack, doc);
    CHECK(stack.count() == 5);
    while (stack.canUndo())
        stack.undo();
    CHECK(doc.bodyCount() == 3);  // the 3 oldest adds can no longer be undone
    CHECK_FALSE(stack.isClean()); // the empty starting state fell off the history
}

TEST_CASE("clean state tracks undo and redo", "[commands]")
{
    Document doc;
    CommandStack stack(doc);
    add(stack, doc);
    stack.setClean();
    add(stack, doc);
    CHECK_FALSE(stack.isClean());
    stack.undo();
    CHECK(stack.isClean());
    stack.undo();
    CHECK_FALSE(stack.isClean());

    // Branching away from the saved state makes it unreachable.
    add(stack, doc);
    stack.undo();
    stack.redo();
    CHECK_FALSE(stack.isClean());
}
