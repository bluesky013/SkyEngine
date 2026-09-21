//
// Created on 2026/09/21.
//

#include <editor/core/command/CommandService.h>
#include <utility>

namespace sky::editor {

    namespace {

        // Groups an already-executed batch of commands into one undo step.
        class TransactionCommand : public UndoCommand {
        public:
            TransactionCommand(std::string name, std::vector<UndoCommandPtr> commands)
                : UndoCommand(std::move(name)), commands(std::move(commands))
            {
            }

            void Do() override
            {
                for (auto &command : commands) {
                    command->Do();
                }
            }

            void Undo() override
            {
                for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
                    (*it)->Undo();
                }
            }

        private:
            std::vector<UndoCommandPtr> commands;
        };

    } // namespace

    void CommandService::Execute(UndoCommandPtr command)
    {
        if (command == nullptr) {
            return;
        }

        command->Do();
        if (transactionDepth > 0) {
            pending.push_back(std::move(command));
        } else {
            PushExecuted(std::move(command));
        }
    }

    bool CommandService::Undo()
    {
        if (undoStack.empty()) {
            return false;
        }

        auto command = std::move(undoStack.back());
        undoStack.pop_back();
        command->Undo();
        redoStack.push_back(std::move(command));
        Notify();
        return true;
    }

    bool CommandService::Redo()
    {
        if (redoStack.empty()) {
            return false;
        }

        auto command = std::move(redoStack.back());
        redoStack.pop_back();
        command->Do();
        undoStack.push_back(std::move(command));
        Notify();
        return true;
    }

    void CommandService::BeginTransaction(std::string name)
    {
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
