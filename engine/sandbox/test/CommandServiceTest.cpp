//
// Created on 2026/09/21.
//

#include <editor/core/command/CommandService.h>
#include <editor/core/command/UndoCommand.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace sky::editor {
    namespace {

        class AddCommand : public UndoCommand {
        public:
            AddCommand(int &target, int delta) : UndoCommand("add"), target(target), delta(delta)
            {
            }

            void Do() override
            {
                target += delta;
            }
            void Undo() override
            {
                target -= delta;
            }

        private:
            int &target;
            int  delta;
        };

        class CallbackCommand : public UndoCommand {
        public:
            CallbackCommand(std::function<void()> action, std::function<void()> inverse)
                : UndoCommand("callback"), action(std::move(action)), inverse(std::move(inverse))
            {
            }
            void Do() override
            {
                action();
            }
            void Undo() override
            {
                inverse();
            }

        private:
            std::function<void()> action;
            std::function<void()> inverse;
        };

    } // namespace

    TEST(CommandServiceTest, ExecuteUndoRedo)
    {
        CommandService service;
        int            value = 0;

        EXPECT_FALSE(service.CanUndo());
        EXPECT_FALSE(service.CanRedo());

        service.Execute(std::make_unique<AddCommand>(value, 5));
        EXPECT_EQ(value, 5);
        EXPECT_TRUE(service.CanUndo());
        EXPECT_EQ(service.GetUndoCount(), 1u);

        EXPECT_TRUE(service.Undo());
        EXPECT_EQ(value, 0);
        EXPECT_TRUE(service.CanRedo());

        EXPECT_TRUE(service.Redo());
        EXPECT_EQ(value, 5);
        EXPECT_FALSE(service.CanRedo());
    }

    TEST(CommandServiceTest, ExecuteClearsRedo)
    {
        CommandService service;
        int            value = 0;

        service.Execute(std::make_unique<AddCommand>(value, 1));
        service.Undo();
        EXPECT_TRUE(service.CanRedo());

        service.Execute(std::make_unique<AddCommand>(value, 2));
        EXPECT_FALSE(service.CanRedo());
        EXPECT_EQ(value, 2);
    }

    TEST(CommandServiceTest, TransactionGroupsAsOneStep)
    {
        CommandService service;
        int            value = 0;

        service.BeginTransaction("batch");
        service.Execute(std::make_unique<AddCommand>(value, 1));
        service.Execute(std::make_unique<AddCommand>(value, 2));
        service.Execute(std::make_unique<AddCommand>(value, 3));
        service.EndTransaction();

        EXPECT_EQ(value, 6);
        EXPECT_EQ(service.GetUndoCount(), 1u);

        EXPECT_TRUE(service.Undo());
        EXPECT_EQ(value, 0);

        EXPECT_TRUE(service.Redo());
        EXPECT_EQ(value, 6);
    }

    TEST(CommandServiceTest, EmptyTransactionIsNoop)
    {
        CommandService service;
        service.BeginTransaction();
        service.EndTransaction();

        EXPECT_FALSE(service.CanUndo());
        EXPECT_EQ(service.GetUndoCount(), 0u);
    }

    TEST(CommandServiceTest, ObserversNotified)
    {
        CommandService service;
        int            value    = 0;
        int            notified = 0;
        const auto     id       = service.AddChangeCallback([&]() { ++notified; });

        service.Execute(std::make_unique<AddCommand>(value, 1));
        EXPECT_EQ(notified, 1);
        service.Undo();
        EXPECT_EQ(notified, 2);
        service.Redo();
        EXPECT_EQ(notified, 3);

        service.RemoveChangeCallback(id);
        service.Undo();
        EXPECT_EQ(notified, 3);
    }

    TEST(CommandServiceTest, HistoryPreparationCancelsPreviewBeforeUndoAndRedo)
    {
        CommandService service;
        int            value = 0;
        service.Execute(std::make_unique<AddCommand>(value, 5));
        int        restore  = 5;
        int        calls    = 0;
        const auto observer = service.AddBeforeHistoryCallback([&]() {
            value = restore;
            ++calls;
        });

        value = 99; // Uncommitted preview over the committed value.
        ASSERT_TRUE(service.Undo());
        EXPECT_EQ(value, 0);
        EXPECT_EQ(calls, 1);
        restore = 0;
        value   = 77;
        ASSERT_TRUE(service.Redo());
        EXPECT_EQ(value, 5);
        EXPECT_EQ(calls, 2);
        service.RemoveBeforeHistoryCallback(observer);
        ASSERT_TRUE(service.Undo());
        EXPECT_EQ(calls, 2);
    }

    TEST(CommandServiceTest, EmptyHistoryStillPreparesAndRecursiveTraversalIsRejected)
    {
        CommandService service;
        int            calls = 0;
        service.AddBeforeHistoryCallback([&]() {
            ++calls;
            EXPECT_FALSE(service.Undo());
            EXPECT_FALSE(service.Redo());
        });
        EXPECT_FALSE(service.Undo());
        EXPECT_FALSE(service.Redo());
        EXPECT_EQ(calls, 2);
        int value = 0;
        service.Execute(std::make_unique<AddCommand>(value, 3));
        ASSERT_TRUE(service.Undo());
        EXPECT_EQ(value, 0);
        EXPECT_EQ(calls, 3);
    }

    TEST(CommandServiceTest, HistoryPreparationSafelyRemovesAndAddsObservers)
    {
        CommandService             service;
        std::vector<int>           order;
        CommandService::CallbackId second = 0;
        CommandService::CallbackId late   = 0;
        const auto                 first  = service.AddBeforeHistoryCallback([&]() {
            order.push_back(1);
            service.RemoveBeforeHistoryCallback(second);
            if (late == 0) {
                late = service.AddBeforeHistoryCallback([&]() { order.push_back(3); });
            }
        });
        second                            = service.AddBeforeHistoryCallback([&]() { order.push_back(2); });
        EXPECT_FALSE(service.Undo());
        EXPECT_EQ(order, (std::vector<int>{1}));
        EXPECT_FALSE(service.Redo());
        EXPECT_EQ(order, (std::vector<int>{1, 1, 3}));
        service.RemoveBeforeHistoryCallback(first);
        service.RemoveBeforeHistoryCallback(late);
    }

    TEST(CommandServiceTest, HistoryIsRecheckedAfterPreparationChangesIt)
    {
        CommandService service;
        int            value = 0;
        service.Execute(std::make_unique<AddCommand>(value, 3));
        service.AddBeforeHistoryCallback([&]() { service.Clear(); });
        EXPECT_FALSE(service.Undo());
        EXPECT_EQ(value, 3);
        EXPECT_EQ(service.GetUndoCount(), 0u);
        EXPECT_EQ(service.GetRedoCount(), 0u);
    }

    TEST(CommandServiceTest, ClearDuringExecuteKeepsOnlyCommandsFromTheNewSession)
    {
        CommandService service;
        int            oldValue = 0;
        int            newValue = 0;
        service.Execute(std::make_unique<CallbackCommand>(
            [&]() {
                ++oldValue;
                service.Clear();
                service.Execute(std::make_unique<AddCommand>(newValue, 2));
            },
            [&]() { --oldValue; }));
        EXPECT_EQ(service.GetUndoCount(), 1u);
        EXPECT_EQ(service.GetRedoCount(), 0u);
        EXPECT_TRUE(service.Undo());
        EXPECT_EQ(newValue, 0);
        EXPECT_EQ(oldValue, 1);
    }

    TEST(CommandServiceTest, ClearDuringUndoOrRedoDoesNotReinsertAnOldSessionCommand)
    {
        for (bool clearOnUndo : {false, true}) {
            CommandService service;
            int            value     = 0;
            bool           clearOnDo = false;
            service.Execute(std::make_unique<CallbackCommand>(
                [&]() {
                    ++value;
                    if (clearOnDo) {
                        service.Clear();
                    }
                },
                [&]() {
                    --value;
                    if (clearOnUndo) {
                        service.Clear();
                    }
                }));
            EXPECT_TRUE(service.Undo());
            EXPECT_EQ(value, 0);
            if (!clearOnUndo) {
                ASSERT_EQ(service.GetRedoCount(), 1u);
                clearOnDo = true;
                EXPECT_TRUE(service.Redo());
                EXPECT_EQ(value, 1);
            }
            EXPECT_EQ(service.GetUndoCount(), 0u);
            EXPECT_EQ(service.GetRedoCount(), 0u);
        }
    }

} // namespace sky::editor
