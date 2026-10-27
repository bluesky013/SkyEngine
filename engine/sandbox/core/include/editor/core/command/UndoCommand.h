//
// Created on 2026/09/21.
//

#pragma once

#include <memory>
#include <string>
#include <utility>

namespace sky::editor {

    // A single undoable operation. Do() applies the change, Undo() reverts it.
    // Commands carry no UI-toolkit or render dependency.
    class UndoCommand {
    public:
        explicit UndoCommand(std::string name = {}) : name(std::move(name))
        {
        }
        virtual ~UndoCommand() = default;

        virtual void Do()   = 0;
        virtual void Undo() = 0;

        // Fallible commands override these; failure must leave the edited state unchanged.
        virtual bool TryDo()
        {
            Do();
            return true;
        }
        virtual bool TryUndo()
        {
            Undo();
            return true;
        }

        const std::string &GetName() const
        {
            return name;
        }

    private:
        std::string name;
    };

    using UndoCommandPtr = std::unique_ptr<UndoCommand>;

} // namespace sky::editor
