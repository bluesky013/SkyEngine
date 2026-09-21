//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Toolkit-independent undo/redo service.
    //
    // Commands are executed immediately when passed to Execute(). A transaction
    // groups several commands into a single user-visible undo step. Observers can
    // subscribe to stack changes so views can update enabled state.
    class CommandService {
    public:
        using ChangeCallback = std::function<void()>;
        using CallbackId = uint32_t;

        CommandService() = default;
        ~CommandService() = default;

        CommandService(const CommandService &) = delete;
        CommandService &operator=(const CommandService &) = delete;

        void Execute(UndoCommandPtr command);

        bool Undo();
        bool Redo();

        void BeginTransaction(std::string name = {});
        void EndTransaction();

        void Clear();

        bool CanUndo() const { return !undoStack.empty(); }
        bool CanRedo() const { return !redoStack.empty(); }
        uint32_t GetUndoCount() const { return static_cast<uint32_t>(undoStack.size()); }
        uint32_t GetRedoCount() const { return static_cast<uint32_t>(redoStack.size()); }

        CallbackId AddChangeCallback(ChangeCallback callback);
        void RemoveChangeCallback(CallbackId id);

    private:
        void PushExecuted(UndoCommandPtr command);
        void Notify();

        std::vector<UndoCommandPtr> undoStack;
        std::vector<UndoCommandPtr> redoStack;

        uint32_t transactionDepth = 0;
        std::string transactionName;
        std::vector<UndoCommandPtr> pending;

        std::unordered_map<CallbackId, ChangeCallback> callbacks;
        CallbackId nextCallbackId = 1;
    };

} // namespace sky::editor
