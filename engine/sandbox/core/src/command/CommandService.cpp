//
// Created on 2026/09/21.
//

#include <algorithm>
#include <editor/core/command/CommandService.h>
#include <utility>

namespace sky::editor {

    namespace {

        class HistoryTraversalScope {
        public:
            explicit HistoryTraversalScope(bool &active) : active(active)
            {
                active = true;
            }
            ~HistoryTraversalScope()
            {
                active = false;
            }

        private:
            bool &active;
        };

        // Groups an already-executed batch of commands into one undo step.
        class TransactionCommand : public UndoCommand {
        public:
            TransactionCommand(std::string name, std::vector<UndoCommandPtr> commands) : UndoCommand(std::move(name)), commands(std::move(commands))
            {
            }

            void Do() override
            {
                TryDo();
            }

            void Undo() override
            {
                TryUndo();
            }

            bool TryDo() override
            {
                size_t done = 0;
                for (; done < commands.size(); ++done) {
                    if (!commands[done]->TryDo()) {
                        while (done > 0) {
                            if (!commands[--done]->TryUndo()) {
                                return false;
                            }
                        }
                        return false;
                    }
                }
                return true;
            }

            bool TryUndo() override
            {
                for (size_t index = commands.size(); index > 0; --index) {
                    if (!commands[index - 1]->TryUndo()) {
                        for (size_t restore = index; restore < commands.size(); ++restore) {
                            if (!commands[restore]->TryDo()) {
                                return false;
                            }
                        }
                        return false;
                    }
                }
                return true;
            }

        private:
            std::vector<UndoCommandPtr> commands;
        };

    } // namespace

    bool CommandService::Execute(UndoCommandPtr command)
    {
        if (command == nullptr) {
            return false;
        }

        const uint64_t generation = historyGeneration;
        ++revision;
        if (!command->TryDo()) {
            return false;
        }
        if (generation != historyGeneration) {
            return true; // A synchronous callback closed/replaced this history session.
        }
        if (transactionDepth > 0) {
            pending.push_back(std::move(command));
        } else {
            PushExecuted(std::move(command));
        }
        return true;
    }

    bool CommandService::Undo()
    {
        if (traversingHistory) {
            return false;
        }
        HistoryTraversalScope traversal(traversingHistory);
        PrepareHistory();
        if (undoStack.empty()) {
            return false;
        }

        const uint64_t generation = historyGeneration;
        auto           command    = std::move(undoStack.back());
        undoStack.pop_back();
        ++revision;
        if (!command->TryUndo()) {
            if (generation == historyGeneration) {
                undoStack.push_back(std::move(command));
            }
            return false;
        }
        if (generation != historyGeneration) {
            return true;
        }
        redoStack.push_back(std::move(command));
        Notify();
        return true;
    }

    bool CommandService::Redo()
    {
        if (traversingHistory) {
            return false;
        }
        HistoryTraversalScope traversal(traversingHistory);
        PrepareHistory();
        if (redoStack.empty()) {
            return false;
        }

        const uint64_t generation = historyGeneration;
        auto           command    = std::move(redoStack.back());
        redoStack.pop_back();
        ++revision;
        if (!command->TryDo()) {
            if (generation == historyGeneration) {
                redoStack.push_back(std::move(command));
            }
            return false;
        }
        if (generation != historyGeneration) {
            return true;
        }
        undoStack.push_back(std::move(command));
        Notify();
        return true;
    }

    void CommandService::BeginTransaction(std::string name)
    {
        ++revision;
        if (transactionDepth == 0) {
            transactionName = std::move(name);
            pending.clear();
        }
        ++transactionDepth;
    }

    void CommandService::EndTransaction()
    {
        if (transactionDepth == 0) {
            return;
        }

        ++revision;
        --transactionDepth;
        if (transactionDepth > 0) {
            return;
        }

        if (pending.empty()) {
            return;
        }

        auto transaction = std::make_unique<TransactionCommand>(std::move(transactionName), std::move(pending));
        pending.clear();
        PushExecuted(std::move(transaction));
    }

    void CommandService::Clear()
    {
        ++historyGeneration;
        ++revision;
        undoStack.clear();
        redoStack.clear();
        pending.clear();
        transactionDepth = 0;
        transactionName.clear();
        Notify();
    }

    CommandService::CallbackId CommandService::AddChangeCallback(ChangeCallback callback)
    {
        const CallbackId id = nextCallbackId++;
        callbacks.emplace(id, std::move(callback));
        return id;
    }

    void CommandService::RemoveChangeCallback(CallbackId id)
    {
        callbacks.erase(id);
    }

    CommandService::CallbackId CommandService::AddBeforeHistoryCallback(BeforeHistoryCallback callback)
    {
        const CallbackId id = nextCallbackId++;
        beforeHistoryCallbacks.emplace(id, std::move(callback));
        return id;
    }

    void CommandService::RemoveBeforeHistoryCallback(CallbackId id)
    {
        beforeHistoryCallbacks.erase(id);
    }

    void CommandService::PrepareHistory()
    {
        // Snapshot closures so observers can subscribe/unsubscribe or destroy a
        // form. Check membership again before dispatching a removed observer.
        std::vector<std::pair<CallbackId, BeforeHistoryCallback>> snapshot(beforeHistoryCallbacks.begin(), beforeHistoryCallbacks.end());
        std::sort(snapshot.begin(), snapshot.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[id, callback] : snapshot) {
            if (beforeHistoryCallbacks.count(id) != 0 && callback) {
                callback();
            }
        }
    }

    void CommandService::PushExecuted(UndoCommandPtr command)
    {
        redoStack.clear();
        undoStack.push_back(std::move(command));
        Notify();
    }

    void CommandService::Notify()
    {
        // Snapshot so callbacks may safely add or remove observers.
        const auto snapshot = callbacks;
        for (const auto &[id, callback] : snapshot) {
            if (callback) {
                callback();
            }
        }
    }

} // namespace sky::editor
