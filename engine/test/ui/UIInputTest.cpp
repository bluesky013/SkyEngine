//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>
#include <ui/widgets/EditBox.h>

namespace sky::ui {

    namespace {

        class PointerProbe : public UIElement {
        public:
            int moveCount = 0;

            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                if (event.action == UIPointerAction::MOVE) {
                    moveCount++;
                }
                return UIEventResult::HANDLED;
            }
        };

    } // namespace

    TEST(UIInputTest, PressFocusesAndTypingInsertsText)
    {
        UIContext context;
        context.SetContentSize(200.0f, 50.0f);

        auto holder = std::make_unique<EditBox>();
        auto *edit = holder.get();
        context.AddChild(std::move(holder));
        edit->SetBounds({0.0f, 0.0f, 200.0f, 30.0f});

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        router.DispatchPointer(down);
        EXPECT_EQ(router.GetFocus(), edit);

        UITextInputEvent text;
        text.text = "Hi";
        EXPECT_EQ(router.DispatchText(text), UIEventResult::HANDLED);
        EXPECT_EQ(edit->GetText(), "Hi");
        EXPECT_EQ(edit->GetCaret(), 2u);
    }

    TEST(UIInputTest, BackspaceRemovesCharacter)
    {
        UIContext context;
        context.SetContentSize(200.0f, 50.0f);

        auto holder = std::make_unique<EditBox>();
        auto *edit = holder.get();
        context.AddChild(std::move(holder));
        edit->SetBounds({0.0f, 0.0f, 200.0f, 30.0f});
        edit->SetText("Hi");
        edit->SetCaret(2);

        UIEventRouter router(context);
        router.SetFocus(edit);

        UIKeyEvent backspace;
        backspace.keyCode = 0x08;
        EXPECT_EQ(router.DispatchKey(backspace), UIEventResult::HANDLED);
        EXPECT_EQ(edit->GetText(), "H");
    }

    TEST(UIInputTest, CaretMovesWithinBounds)
    {
        EditBox edit;
        edit.SetText("abc");
        edit.SetCaret(1);

        UIKeyEvent left;
        left.keyCode = 0x25;
        edit.OnKeyEvent(left);
        EXPECT_EQ(edit.GetCaret(), 0u);

        edit.OnKeyEvent(left);
        EXPECT_EQ(edit.GetCaret(), 0u);

        UIKeyEvent right;
        right.keyCode = 0x27;
        edit.OnKeyEvent(right);
        edit.OnKeyEvent(right);
        edit.OnKeyEvent(right);
        EXPECT_EQ(edit.GetCaret(), 3u);
    }

    TEST(UIInputTest, MultiPointerIndependentCapture)
    {
        UIContext context;
        context.SetContentSize(200.0f, 200.0f);

        auto firstHolder = std::make_unique<PointerProbe>();
        auto *first = firstHolder.get();
        first->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        context.AddChild(std::move(firstHolder));

        auto secondHolder = std::make_unique<PointerProbe>();
        auto *second = secondHolder.get();
        second->SetBounds({100.0f, 0.0f, 150.0f, 50.0f});
        context.AddChild(std::move(secondHolder));

        UIEventRouter router(context);

        UIPointerEvent downFirst;
        downFirst.action = UIPointerAction::DOWN;
        downFirst.pointerId = 1;
        downFirst.x = 10.0f;
        downFirst.y = 10.0f;
        router.DispatchPointer(downFirst);

        UIPointerEvent downSecond;
        downSecond.action = UIPointerAction::DOWN;
        downSecond.pointerId = 2;
        downSecond.x = 110.0f;
        downSecond.y = 10.0f;
        router.DispatchPointer(downSecond);

        EXPECT_EQ(router.GetCapture(1), first);
        EXPECT_EQ(router.GetCapture(2), second);

        UIPointerEvent moveFirst;
        moveFirst.action = UIPointerAction::MOVE;
        moveFirst.pointerId = 1;
        moveFirst.x = 180.0f;
        moveFirst.y = 180.0f;
        router.DispatchPointer(moveFirst);

        EXPECT_EQ(first->moveCount, 1);
        EXPECT_EQ(second->moveCount, 0);

        UIPointerEvent upFirst;
        upFirst.action = UIPointerAction::UP;
        upFirst.pointerId = 1;
        upFirst.x = 180.0f;
        upFirst.y = 180.0f;
        router.DispatchPointer(upFirst);

        EXPECT_EQ(router.GetCapture(1), nullptr);
        EXPECT_EQ(router.GetCapture(2), second);
    }

} // namespace sky::ui
