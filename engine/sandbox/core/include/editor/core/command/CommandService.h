//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <editor/core/command/UndoCommand.h>
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
        using ChangeCallback        = std::function<void()>;
        using BeforeHistoryCallback = std::function<void()>;
        using CallbackId            = uint32_t;

        CommandService()  = default;
        ~CommandService() = default;

        CommandService(const CommandService &)            = delete;
        CommandService &operator=(const CommandService &) = delete;

        bool Execute(UndoCommandPtr command);

        bool Undo();
        bool Redo();

        void BeginTransaction(std::string name = {});
        void EndTransaction();

        void Clear();

        bool CanUndo() const
        {
            return !undoStack.empty();
        }
        bool CanRedo() const
        {
            return !redoStack.empty();
        }
        uint32_t GetUndoCount() const
        {
            return static_cast<uint32_t>(undoStack.size());
        }
        uint32_t GetRedoCount() const
        {
            return static_cast<uint32_t>(redoStack.size());
        }
        // Changes even for edits inside an open transaction. Hosts can detect
        // synchronous callback edits without relying on stack counts.
        uint64_t GetRevision() const
        {
            return revision;
        }

        CallbackId AddChangeCallback(ChangeCallback callback);
        void       RemoveChangeCallback(CallbackId id);

        // Runs before either history traversal, including an empty stack. Views
        // can cancel uncommitted interactions here without introducing a UI
        // dependency into the service. Callbacks run in registration order;
        // removed callbacks are skipped and recursive Undo/Redo returns false.
        CallbackId AddBeforeHistoryCallback(BeforeHistoryCallback callback);
        void       RemoveBeforeHistoryCallback(CallbackId id);

    private:
        void PushExecuted(UndoCommandPtr command);
        void Notify();
        void PrepareHistory();

        std::vector<UndoCommandPtr> undoStack;
        std::vector<UndoCommandPtr> redoStack;

        uint32_t                    transactionDepth = 0;
        std::string                 transactionName;
        std::vector<UndoCommandPtr> pending;

        std::unordered_map<CallbackId, ChangeCallback>        callbacks;
        std::unordered_map<CallbackId, BeforeHistoryCallback> beforeHistoryCallbacks;
        CallbackId                                            nextCallbackId    = 1;
        bool                                                  traversingHistory = false;
        uint64_t                                              historyGeneration = 0;
        uint64_t                                              revision          = 0;
    };

} // namespace sky::editor
