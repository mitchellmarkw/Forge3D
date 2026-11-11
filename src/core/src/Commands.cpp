// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Commands.h"

#include "forge/core/Document.h"

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
