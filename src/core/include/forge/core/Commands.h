// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace forge {

class Document;

// One undoable change to a Document.
//
// Rules for writing a command:
//  - redo() makes the change; undo() puts the document back exactly as it was.
//  - redo() is called for the first time by CommandStack::execute(), and again
//    after each undo(). Capture any "before" state on the first redo().
//  - If redo() throws, it must not have changed the document.
class Command {
public:
    virtual ~Command() = default;

    virtual std::string label() const = 0;  // shown as "Undo <label>"
    virtual void redo(Document& doc) = 0;
    virtual void undo(Document& doc) = 0;

    // Consecutive commands with the same non-negative mergeId may be combined
    // into one undo step (e.g. every mouse move of one drag). Return true from
    // mergeWith() to absorb `next`, which has already been applied.
    virtual int mergeId() const { return -1; }
    virtual bool mergeWith(const Command& next)
    {
        (void)next;
        return false;
    }
};

// Merge ids in use (keep unique).
enum MergeIds : int { kMergeSetPlacement = 1 };

// ---- Concrete commands ----------------------------------------------------------

class AddBodyCommand final : public Command {
public:
    explicit AddBodyCommand(Body body);
    std::string label() const override;
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    ObjectId id() const { return m_body.id; }

private:
    Body m_body;
};

class RemoveBodyCommand final : public Command {
public:
    explicit RemoveBodyCommand(ObjectId id);
    std::string label() const override;
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    ObjectId m_id;
    std::optional<Body> m_removed;
    std::size_t m_index = 0;
};

class SetPlacementCommand final : public Command {
public:
    // mergeable: true for drags (every mouse move joins one undo step);
    // false for typed values, where each edit should be its own step.
    SetPlacementCommand(ObjectId id, const Placement& placement, bool mergeable = true);
    std::string label() const override { return "Move"; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    int mergeId() const override { return m_mergeable ? kMergeSetPlacement : -1; }
    bool mergeWith(const Command& next) override;

private:
    ObjectId m_id;
    Placement m_new;
    std::optional<Placement> m_old;
    bool m_mergeable;
};

class SetSpecCommand final : public Command {
public:
    SetSpecCommand(ObjectId id, const PrimitiveSpec& spec);
    std::string label() const override;
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    ObjectId m_id;
    PrimitiveSpec m_new;
    std::optional<PrimitiveSpec> m_old;
};

class RenameBodyCommand final : public Command {
public:
    RenameBodyCommand(ObjectId id, std::string name);
    std::string label() const override { return "Rename"; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    ObjectId m_id;
    std::string m_new;
    std::optional<std::string> m_old;
};

// Sets a body's size and placement in one step, with both states given up front.
// Used by gizmo drags: the drag previews changes live, then records one of these.
class EditBodyCommand final : public Command {
public:
    EditBodyCommand(Body before, Body after, std::string label);
    std::string label() const override { return m_label; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    static void apply(Document& doc, const Body& state);
    Body m_before;
    Body m_after;
    std::string m_label;
};

// Changes several bodies in one step (e.g. resizing a combined shape changes all its
// parts). `before` and `after` hold the same bodies, in the same order.
class EditBodiesCommand final : public Command {
public:
    EditBodiesCommand(std::vector<Body> before, std::vector<Body> after, std::string label);
    std::string label() const override { return m_label; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    std::vector<Body> m_before, m_after;
    std::string m_label;
};

// Copies of a body and everything inside it, with new ids and names ("Box 1" ->
// "Box 1 copy"), in the order they can be inserted (parts before the results that
// hold them; the copy of `id` itself last). Nothing is added to the document.
std::vector<Body> duplicateTree(Document& doc, ObjectId id);

// Makes a combine result from top-level bodies (Merge / Subtract / Intersect).
// `result` must have boolean set; its parts disappear from the plate into it.
class CombineCommand final : public Command {
public:
    explicit CombineCommand(Body result);
    std::string label() const override;
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    ObjectId id() const { return m_result.id; }

private:
    Body m_result;
    std::optional<std::size_t> m_index;
};

// Breaks a top-level combine result back into its parts, each left exactly
// where it appeared on the plate.
class SeparateCommand final : public Command {
public:
    explicit SeparateCommand(ObjectId id);
    std::string label() const override { return "Separate"; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    ObjectId m_id;
    std::optional<Body> m_result;
    std::size_t m_index = 0;
    std::vector<std::pair<ObjectId, Placement>> m_oldPlacements;
};

// Several commands that undo and redo as one step ("Clear Plate").
// Puts a new step into an object's history, between `above` and its first part (an
// Edges or Emboss step added at the rollback bar).
class InsertStepCommand final : public Command {
public:
    InsertStepCommand(Body step, ObjectId above);
    std::string label() const override;
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    Body m_step;
    ObjectId m_above;
};

// Deletes a step from inside an object's history: the shape it worked on takes its
// place; any shapes it used as tools go back on the plate.
class RemoveStepCommand final : public Command {
public:
    explicit RemoveStepCommand(ObjectId step);
    std::string label() const override { return "Delete step"; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

private:
    ObjectId m_id;
    ObjectId m_above = kInvalidId;
    Body m_step;
    std::vector<Body> m_tools;  // their placements as they were
};

class MacroCommand final : public Command {
public:
    explicit MacroCommand(std::string label);
    std::string label() const override { return m_label; }
    void redo(Document& doc) override;
    void undo(Document& doc) override;

    // Adds a command that has ALREADY been applied (used while recording).
    void appendApplied(std::unique_ptr<Command> command);
    bool empty() const { return m_children.empty(); }
    std::size_t size() const { return m_children.size(); }

private:
    std::string m_label;
    std::vector<std::unique_ptr<Command>> m_children;
};

}  // namespace forge
