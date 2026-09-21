//
// Created on 2026/09/21.
//

#include <editor/core/command/CommandService.h>
#include <editor/core/command/UndoCommand.h>
#include <gtest/gtest.h>
#include <memory>

namespace sky::editor {
    namespace {

        class AddCommand : public UndoCommand {
        public:
            AddCommand(int &target, int delta) : UndoCommand("add"), target(target), delta(delta) {}

            void Do() override { target += delta; }
            void Undo() override { target -= delta; }

        private:
            int &target;
            int delta;
        };

    } // namespace

    TEST(CommandServiceTest, ExecuteUndoRedo)
    {
        CommandService service;
        int value = 0;

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
        int value = 0;

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
        int value = 0;

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
        int value = 0;
        int notified = 0;
        const auto id = service.AddChangeCallback([&]() { ++notified; });

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

} // namespace sky::editor
