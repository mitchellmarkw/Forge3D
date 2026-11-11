// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/CommandStack.h"

#include "forge/core/Document.h"

#include <stdexcept>
#include <utility>

namespace forge {

CommandStack::CommandStack(Document& doc, std::size_t limit) : m_doc(doc), m_limit(limit < 1 ? 1 : limit) {}

void CommandStack::execute(std::unique_ptr<Command> command)
{
    if (!command)
        throw std::invalid_argument("CommandStack::execute: null command");
    command->redo(m_doc);  // throws => nothing recorded
    record(std::move(command));
}

void CommandStack::record(std::unique_ptr<Command> command)
{
    if (!m_openMacros.empty()) {
        m_openMacros.back()->appendApplied(std::move(command));
        return;  // listeners hear about it when the macro closes
    }

    // A new edit discards the redo branch.
    if (m_index < m_commands.size()) {
        m_commands.erase(m_commands.begin() + static_cast<std::ptrdiff_t>(m_index), m_commands.end());
        if (m_cleanIndex > static_cast<long long>(m_index))
            m_cleanIndex = -1;
    }

    // Merge into the previous step (e.g. one undo for a whole drag), unless that
    // step is the saved state: then the saved state must stay reachable.
    if (m_index > 0 && command->mergeId() >= 0 && m_cleanIndex != static_cast<long long>(m_index)) {
        Command& top = *m_commands[m_index - 1];
        if (top.mergeId() == command->mergeId() && top.mergeWith(*command)) {
            changed();
            return;
        }
    }

    m_commands.push_back(std::move(command));
    ++m_index;

    if (m_commands.size() > m_limit) {
        m_commands.erase(m_commands.begin());
        --m_index;
        m_cleanIndex = m_cleanIndex > 0 ? m_cleanIndex - 1 : -1;
    }
    changed();
}

bool CommandStack::canUndo() const
{
    return m_index > 0 && m_openMacros.empty();
}

bool CommandStack::canRedo() const
{
    return m_index < m_commands.size() && m_openMacros.empty();
}

void CommandStack::undo()
{
    if (!canUndo())
        return;
    m_commands[m_index - 1]->undo(m_doc);
    --m_index;
    changed();
}

void CommandStack::redo()
{
    if (!canRedo())
        return;
    m_commands[m_index]->redo(m_doc);
    ++m_index;
    changed();
}

std::string CommandStack::undoLabel() const
{
    return canUndo() ? m_commands[m_index - 1]->label() : std::string();
}

std::string CommandStack::redoLabel() const
{
    return canRedo() ? m_commands[m_index]->label() : std::string();
}

void CommandStack::beginMacro(const std::string& label)
{
    m_openMacros.push_back(std::make_unique<MacroCommand>(label));
}

void CommandStack::endMacro()
{
    if (m_openMacros.empty())
        throw std::logic_error("CommandStack::endMacro without beginMacro");
    std::unique_ptr<MacroCommand> macro = std::move(m_openMacros.back());
    m_openMacros.pop_back();
    if (macro->empty()) {
        if (m_openMacros.empty())
            changed();
        return;
    }
    record(std::move(macro));  // into the parent macro, or onto the stack
}

void CommandStack::setClean()
{
    m_cleanIndex = static_cast<long long>(m_index);
    changed();
}

bool CommandStack::isClean() const
{
    return m_cleanIndex == static_cast<long long>(m_index);
}

void CommandStack::clear()
{
    m_commands.clear();
    m_openMacros.clear();
    m_index = 0;
    m_cleanIndex = 0;
    changed();
}

void CommandStack::changed()
{
    if (m_listener)
        m_listener();
}

}  // namespace forge
