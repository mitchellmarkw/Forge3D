// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Commands.h"

#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <stdexcept>
#include <utility>

namespace forge {

// ---- AddBodyCommand -------------------------------------------------------------

AddBodyCommand::AddBodyCommand(Body body) : m_body(std::move(body)) {}

std::string AddBodyCommand::label() const
{
    return "Add " + typeName(m_body.spec);
}

void AddBodyCommand::redo(Document& doc)
{
    doc.insertBody(m_body);
}

void AddBodyCommand::undo(Document& doc)
{
    // Keep any changes made to the body since (there are none while this command
    // is on top of the stack, but this keeps redo exact if that ever changes).
    m_body = doc.removeBody(m_body.id);
}

// ---- RemoveBodyCommand ----------------------------------------------------------

RemoveBodyCommand::RemoveBodyCommand(ObjectId id) : m_id(id) {}

std::string RemoveBodyCommand::label() const
{
    return m_removed ? "Delete " + m_removed->name : std::string("Delete");
}

void RemoveBodyCommand::redo(Document& doc)
{
    const std::size_t index = doc.indexOf(m_id);
    if (index == Document::npos)
        throw std::out_of_range("Cannot delete: no body with id " + std::to_string(m_id));
    m_index = index;
    m_removed = doc.removeBody(m_id);
}

void RemoveBodyCommand::undo(Document& doc)
{
    doc.insertBody(*m_removed, m_index);  // same id, same position in the list
}

// ---- SetPlacementCommand --------------------------------------------------------

SetPlacementCommand::SetPlacementCommand(ObjectId id, const Placement& placement, bool mergeable)
    : m_id(id), m_new(placement), m_mergeable(mergeable)
{
}

void SetPlacementCommand::redo(Document& doc)
{
    const Body* body = doc.find(m_id);
    if (!body)
        throw std::out_of_range("Cannot move: no body with id " + std::to_string(m_id));
    if (!m_old)
        m_old = body->placement;
    doc.setPlacement(m_id, m_new);
}

void SetPlacementCommand::undo(Document& doc)
{
    doc.setPlacement(m_id, *m_old);
}

bool SetPlacementCommand::mergeWith(const Command& next)
{
    const auto* other = dynamic_cast<const SetPlacementCommand*>(&next);
    if (!other || other->m_id != m_id || !m_mergeable || !other->m_mergeable)
        return false;
    m_new = other->m_new;  // keep our original "before", take their "after"
    return true;
}

// ---- SetSpecCommand -------------------------------------------------------------

SetSpecCommand::SetSpecCommand(ObjectId id, const PrimitiveSpec& spec) : m_id(id), m_new(spec) {}

std::string SetSpecCommand::label() const
{
    return "Resize " + typeName(m_new);
}

void SetSpecCommand::redo(Document& doc)
{
    const Body* body = doc.find(m_id);
    if (!body)
        throw std::out_of_range("Cannot resize: no body with id " + std::to_string(m_id));
    const PrimitiveSpec before = body->spec;
    doc.setSpec(m_id, m_new);  // validates first; throws without changing anything
    if (!m_old)
        m_old = before;
}

void SetSpecCommand::undo(Document& doc)
{
    doc.setSpec(m_id, *m_old);
}

// ---- RenameBodyCommand ----------------------------------------------------------

RenameBodyCommand::RenameBodyCommand(ObjectId id, std::string name) : m_id(id), m_new(std::move(name)) {}

void RenameBodyCommand::redo(Document& doc)
{
    const Body* body = doc.find(m_id);
    if (!body)
        throw std::out_of_range("Cannot rename: no body with id " + std::to_string(m_id));
    const std::string before = body->name;
    doc.rename(m_id, m_new);
    if (!m_old)
        m_old = before;
}

void RenameBodyCommand::undo(Document& doc)
{
    doc.rename(m_id, *m_old);
}

// ---- EditBodyCommand ------------------------------------------------------------

EditBodyCommand::EditBodyCommand(Body before, Body after, std::string label)
    : m_before(std::move(before)), m_after(std::move(after)), m_label(std::move(label))
{
    if (m_before.id != m_after.id)
        throw std::invalid_argument("EditBodyCommand: before and after must be the same body");
}

void EditBodyCommand::apply(Document& doc, const Body& state)
{
    const Body* current = doc.find(state.id);
    if (!current)
        throw std::out_of_range("Cannot edit: no body with id " + std::to_string(state.id));
    validate(state.spec);  // check before touching anything
    const PrimitiveSpec oldSpec = current->spec;
    doc.setSpec(state.id, state.spec);
    try {
        doc.setPlacement(state.id, state.placement);
    } catch (...) {
        doc.setSpec(state.id, oldSpec);  // leave the document as it was
        throw;
    }
}

void EditBodyCommand::redo(Document& doc)
{
    apply(doc, m_after);
}

void EditBodyCommand::undo(Document& doc)
{
    apply(doc, m_before);
}

// ---- CombineCommand -------------------------------------------------------------

CombineCommand::CombineCommand(Body result) : m_result(std::move(result))
{
    if (!m_result.isBoolean())
        throw std::invalid_argument("CombineCommand needs a combine result");
}

std::string CombineCommand::label() const
{
    return toString(m_result.boolean->op);
}

void CombineCommand::redo(Document& doc)
{
    for (ObjectId part : m_result.boolean->operands) {
        if (!doc.find(part))
            throw std::out_of_range("Cannot combine: a shape is missing");
        if (!doc.isTopLevel(part))
            throw std::invalid_argument("Cannot combine: '" + doc.find(part)->name +
                                        "' is already part of another shape");
    }
    if (!m_index)  // put the result where its first part was in the list
        m_index = doc.indexOf(m_result.boolean->operands.front());
    doc.insertBody(m_result, *m_index);
}

void CombineCommand::undo(Document& doc)
{
    m_result = doc.removeBody(m_result.id);  // frees the parts
}

// ---- SeparateCommand ------------------------------------------------------------

SeparateCommand::SeparateCommand(ObjectId id) : m_id(id) {}

void SeparateCommand::redo(Document& doc)
{
    const Body* result = doc.find(m_id);
    if (!result || !result->isBoolean())
        throw std::invalid_argument("Only a combined shape can be separated");
    if (!doc.isTopLevel(m_id))
        throw std::invalid_argument("Separate '" + doc.find(doc.parentOf(m_id))->name + "' first");

    // Where each part is on the plate now, as a placement of its own.
    const Transform resultT = doc.localTransform(m_id);
    std::vector<std::pair<ObjectId, Placement>> oldPlacements, newPlacements;
    for (ObjectId part : result->boolean->operands) {
        const Body* b = doc.find(part);
        oldPlacements.emplace_back(part, b->placement);
        newPlacements.emplace_back(part, placementFromTransform(resultT * doc.localTransform(part), pivotOf(*b)));
    }

    m_index = doc.indexOf(m_id);
    m_result = doc.removeBody(m_id);
    for (const auto& [part, placement] : newPlacements)
        doc.setPlacement(part, placement);
    m_oldPlacements = std::move(oldPlacements);
}

void SeparateCommand::undo(Document& doc)
{
    for (const auto& [part, placement] : m_oldPlacements)
        doc.setPlacement(part, placement);
    doc.insertBody(*m_result, m_index);
}

// ---- MacroCommand ---------------------------------------------------------------

MacroCommand::MacroCommand(std::string label) : m_label(std::move(label)) {}

void MacroCommand::appendApplied(std::unique_ptr<Command> command)
{
    m_children.push_back(std::move(command));
}

void MacroCommand::redo(Document& doc)
{
    std::size_t done = 0;
    try {
        for (; done < m_children.size(); ++done)
            m_children[done]->redo(doc);
    } catch (...) {
        // Roll back the part that succeeded so the document is unchanged.
        while (done > 0)
            m_children[--done]->undo(doc);
        throw;
    }
}

void MacroCommand::undo(Document& doc)
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        (*it)->undo(doc);
}

}  // namespace forge
