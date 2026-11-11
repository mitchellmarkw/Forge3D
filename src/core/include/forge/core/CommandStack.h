// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Commands.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace forge {

class Document;

// The single undo history for a document. Every edit, in every mode, goes
// through execute(), so Ctrl+Z always undoes the last thing the user did.
class CommandStack {
public:
    using Listener = std::function<void()>;

    explicit CommandStack(Document& doc, std::size_t limit = 200);
    CommandStack(const CommandStack&) = delete;
    CommandStack& operator=(const CommandStack&) = delete;

    // Applies the command and records it. Anything that could be redone is
    // discarded. If the command throws, nothing is recorded and the exception
    // propagates (the document is unchanged).
    void execute(std::unique_ptr<Command> command);

    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    std::string undoLabel() const;  // empty if nothing to undo
    std::string redoLabel() const;

    // Groups everything executed until endMacro() into one undo step.
    // Macros may nest; only the outermost one is recorded. An empty macro is dropped.
    void beginMacro(const std::string& label);
    void endMacro();
    bool inMacro() const { return !m_openMacros.empty(); }

    // "Clean" = matches what is saved on disk. Used for the * in the title bar.
    void setClean();
    bool isClean() const;

    void clear();
    std::size_t count() const { return m_commands.size(); }  // undo + redo steps held
    std::size_t index() const { return m_index; }            // undo steps available
    std::size_t limit() const { return m_limit; }

    void setListener(Listener listener) { m_listener = std::move(listener); }

private:
    void record(std::unique_ptr<Command> command);
    void changed();

    Document& m_doc;
    std::size_t m_limit;
    std::vector<std::unique_ptr<Command>> m_commands;
    std::size_t m_index = 0;  // commands [0, m_index) are applied
    // Index at which the document was clean; -1 when that state can no longer be
    // reached by undo/redo.
    long long m_cleanIndex = 0;
    std::vector<std::unique_ptr<MacroCommand>> m_openMacros;
    Listener m_listener;
};

}  // namespace forge
