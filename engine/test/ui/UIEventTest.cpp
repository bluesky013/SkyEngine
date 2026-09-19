//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>

namespace sky::ui {

    namespace {

        class TestElement : public UIElement {
        public:
            int pointerCount = 0;
            int keyCount = 0;
            int enterCount = 0;
            int leaveCount = 0;
            bool handlePointer = true;

            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                (void)event;
                pointerCount++;
                return handlePointer ? UIEventResult::HANDLED : UIEventResult::UNHANDLED;
            }

            UIEventResult OnKeyEvent(const UIKeyEvent &event) override
            {
                (void)event;
                keyCount++;
                return UIEventResult::HANDLED;
            }

            void OnPointerEnter(const UIPointerEvent &event) override
            {
                (void)event;
                enterCount++;
            }

            void OnPointerLeave(const UIPointerEvent &event) override
            {
                (void)event;
                leaveCount++;
            }
        };

    } // namespace

    TEST(UIEventTest, TopmostElementReceivesPointer)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto *lower = context.AddChild(std::make_unique<TestElement>());
        auto *upper = context.AddChild(std::make_unique<TestElement>());
        lower->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        upper->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});

        UIEventRouter router(context);
        EXPECT_EQ(router.HitTest(10.0f, 10.0f), upper);
        EXPECT_NE(router.HitTest(10.0f, 10.0f), lower);
    }

    TEST(UIEventTest, ClipExcludesElement)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto *parent = context.AddChild(std::make_unique<TestElement>());
        parent->SetClipsChildren(true);
        parent->SetBounds({0.0f, 0.0f, 30.0f, 30.0f});

        auto *child = parent->AddChild(std::make_unique<TestElement>());
        child->SetBounds({50.0f, 50.0f, 80.0f, 80.0f});

        UIEventRouter router(context);
        EXPECT_NE(router.HitTest(60.0f, 60.0f), child);
    }

    TEST(UIEventTest, FocusReceivesKeyboard)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto holder = std::make_unique<TestElement>();
        auto *element = holder.get();
        context.AddChild(std::move(holder));

        UIEventRouter router(context);
        router.SetFocus(element);

        UIKeyEvent key;
        EXPECT_EQ(router.DispatchKey(key), UIEventResult::HANDLED);
        EXPECT_EQ(element->keyCount, 1);
    }

    TEST(UIEventTest, CaptureHoldsDrag)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto holder = std::make_unique<TestElement>();
        auto *element = holder.get();
        context.AddChild(std::move(holder));
        element->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        router.DispatchPointer(down);
        EXPECT_EQ(element->pointerCount, 1);
        EXPECT_EQ(router.GetCapture(), element);

        UIPointerEvent move;
        move.action = UIPointerAction::MOVE;
        move.x = 90.0f;
        move.y = 90.0f;
        router.DispatchPointer(move);
        EXPECT_EQ(element->pointerCount, 2);

        UIPointerEvent up;
        up.action = UIPointerAction::UP;
        up.x = 90.0f;
        up.y = 90.0f;
        router.DispatchPointer(up);
        EXPECT_EQ(element->pointerCount, 3);
        EXPECT_EQ(router.GetCapture(), nullptr);
    }

    TEST(UIEventTest, HoverEnterAndLeave)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto firstHolder = std::make_unique<TestElement>();
        auto *first = firstHolder.get();
        first->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        context.AddChild(std::move(firstHolder));

        auto secondHolder = std::make_unique<TestElement>();
        auto *second = secondHolder.get();
        second->SetBounds({50.0f, 0.0f, 100.0f, 50.0f});
        context.AddChild(std::move(secondHolder));

        UIEventRouter router(context);

        UIPointerEvent move;
        move.action = UIPointerAction::MOVE;
        move.x = 10.0f;
        move.y = 10.0f;
        router.DispatchPointer(move);
        EXPECT_EQ(first->enterCount, 1);

        move.x = 60.0f;
        router.DispatchPointer(move);
        EXPECT_EQ(first->leaveCount, 1);
        EXPECT_EQ(second->enterCount, 1);
    }

    TEST(UIEventTest, UnhandledBubblesToParent)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto parentHolder = std::make_unique<TestElement>();
        auto *parent = parentHolder.get();
        parent->SetBounds({0.0f, 0.0f, 100.0f, 100.0f});
        context.AddChild(std::move(parentHolder));

        auto childHolder = std::make_unique<TestElement>();
        auto *child = childHolder.get();
        child->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        child->handlePointer = false;
        parent->AddChild(std::move(childHolder));

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        EXPECT_EQ(router.DispatchPointer(down), UIEventResult::HANDLED);
        EXPECT_EQ(child->pointerCount, 1);
        EXPECT_EQ(parent->pointerCount, 1);
    }

    TEST(UIEventTest, FocusTraversal)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto first = std::make_unique<TestElement>();
        first->SetFocusable(true);
        auto *firstPtr = context.AddChild(std::move(first));

        auto second = std::make_unique<TestElement>();
        second->SetFocusable(true);
        auto *secondPtr = context.AddChild(std::move(second));

        EXPECT_EQ(context.FindNextFocus(nullptr, true), firstPtr);
        EXPECT_EQ(context.FindNextFocus(firstPtr, true), secondPtr);
        EXPECT_EQ(context.FindNextFocus(secondPtr, true), firstPtr);
        EXPECT_EQ(context.FindNextFocus(firstPtr, false), secondPtr);
    }

    TEST(UIEventTest, WantsInputFlag)
    {
        UIContext context;
        EXPECT_FALSE(context.WantsInput());

        context.SetWantsInput(true);
        EXPECT_TRUE(context.WantsInput());
    }

} // namespace sky::ui
