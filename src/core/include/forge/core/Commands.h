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

// Several commands that undo and redo as one step ("Clear Plate").
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
