// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Commands.h"

#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <set>
#include <functional>
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
    const std::optional<BooleanSpec> oldBoolean = current->boolean;
    if (state.boolean.has_value() != oldBoolean.has_value())
        throw std::invalid_argument("Cannot edit: a body can't change between primitive and combined");
    doc.setSpec(state.id, state.spec);
    try {
        if (state.boolean)
            doc.setBoolean(state.id, *state.boolean);
        doc.setStepState(state.id, state.suppressed, state.rolledBack);
        doc.setPlacement(state.id, state.placement);
    } catch (...) {
        doc.setSpec(state.id, oldSpec);  // leave the document as it was
        if (oldBoolean)
            doc.setBoolean(state.id, *oldBoolean);
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

// ---- EditBodiesCommand ---------------------------------------------------------------

EditBodiesCommand::EditBodiesCommand(std::vector<Body> before, std::vector<Body> after, std::string label)
    : m_before(std::move(before)), m_after(std::move(after)), m_label(std::move(label))
{
    if (m_before.size() != m_after.size())
        throw std::invalid_argument("EditBodiesCommand: before and after must list the same bodies");
}

void EditBodiesCommand::redo(Document& doc)
{
    std::size_t done = 0;
    try {
        for (; done < m_after.size(); ++done)
            EditBodyCommand(m_before[done], m_after[done], m_label).redo(doc);
    } catch (...) {
        for (std::size_t i = done; i-- > 0;)  // put back what already changed
            EditBodyCommand(m_before[i], m_after[i], m_label).undo(doc);
        throw;
    }
}

void EditBodiesCommand::undo(Document& doc)
{
    for (std::size_t i = m_before.size(); i-- > 0;)
        EditBodyCommand(m_before[i], m_after[i], m_label).undo(doc);
}

// ---- duplicateTree ---------------------------------------------------------------

std::vector<Body> duplicateTree(Document& doc, ObjectId id)
{
    std::vector<Body> out;
    std::set<std::string> taken;
    const std::function<ObjectId(ObjectId)> copy = [&](ObjectId original) -> ObjectId {
        const Body* b = doc.find(original);
        if (!b)
            throw std::out_of_range("No body with id " + std::to_string(original));
        Body c = *b;
        c.id = doc.allocateId();
        // A name not used in the document or by an earlier copy.
        const auto used = [&](const std::string& n) {
            if (taken.count(n))
                return true;
            for (const Body& other : doc.bodies())
                if (other.name == n)
                    return true;
            return false;
        };
        std::string name = b->name + " copy";
        for (int n = 2; used(name); ++n)
            name = b->name + " copy " + std::to_string(n);
        taken.insert(name);
        c.name = name;
        if (c.boolean)
            for (ObjectId& part : c.boolean->operands)
                part = copy(part);  // parts first
        out.push_back(c);
        return c.id;
    };
    copy(id);
    return out;
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

namespace forge {

// ---- InsertStepCommand / RemoveStepCommand --------------------------------------------

InsertStepCommand::InsertStepCommand(Body step, ObjectId above) : m_step(std::move(step)), m_above(above) {}

std::string InsertStepCommand::label() const
{
    return typeName(m_step);
}

void InsertStepCommand::redo(Document& doc)
{
    doc.insertStep(m_step, m_above);
}

void InsertStepCommand::undo(Document& doc)
{
    doc.removeStep(m_step.id);
}

RemoveStepCommand::RemoveStepCommand(ObjectId step) : m_id(step) {}

void RemoveStepCommand::redo(Document& doc)
{
    m_above = doc.parentOf(m_id);
    const Body* step = doc.find(m_id);
    if (!step)
        throw std::out_of_range("No step with id " + std::to_string(m_id));
    m_tools.clear();
    for (std::size_t i = 1; step->isBoolean() && i < step->boolean->operands.size(); ++i)
        m_tools.push_back(*doc.find(step->boolean->operands[i]));
    m_step = doc.removeStep(m_id);
}

void RemoveStepCommand::undo(Document& doc)
{
    // The tools are on the plate again; put them back as they were, then the step.
    for (const Body& tool : m_tools)
        doc.setPlacement(tool.id, tool.placement);
    doc.insertStep(m_step, m_above);
}

}  // namespace forge
