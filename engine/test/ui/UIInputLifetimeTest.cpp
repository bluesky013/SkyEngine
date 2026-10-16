#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>
#include <ui/widgets/Button.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/Overlay.h>
#include <ui/widgets/StackPanel.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace sky::ui {

    namespace {

        struct InputLifetimeState {
            int                   pointerCount      = 0;
            int                   keyCount          = 0;
            int                   textCount         = 0;
            int                   enterCount        = 0;
            int                   leaveCount        = 0;
            int                   destroyCount      = 0;
            int                   captureQueryCount = 0;
            std::vector<uint32_t> cancellations;
            std::vector<uint32_t> deliveredPointerIds;
            UIEventResult         pointerResult = UIEventResult::HANDLED;
            UIEventResult         keyResult     = UIEventResult::HANDLED;
            UIEventResult         textResult    = UIEventResult::HANDLED;
            std::function<void()> onPointer;
            std::function<void()> onKey;
            std::function<void()> onText;
            std::function<void()> onEnter;
            std::function<void()> onLeave;
            std::function<void()> onCancel;
            std::function<void()> onCaptureQuery;
            bool                  wantsCapture = false;
        };

        // Every callback may destroy this element. Keep the probe's bookkeeping
        // outside the tree and copy user code before calling it, just as a host
        // must avoid reading its own members after a destructive callback.
        class InputLifetimeProbe : public UIElement {
        public:
            explicit InputLifetimeProbe(std::shared_ptr<InputLifetimeState> value) : state(std::move(value))
            {
                SetFocusable(true);
            }

            ~InputLifetimeProbe() override
            {
                ++state->destroyCount;
            }

            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                auto snapshot = state;
                ++snapshot->pointerCount;
                snapshot->deliveredPointerIds.push_back(event.pointerId);
                auto       callback = snapshot->onPointer;
                const auto result   = snapshot->pointerResult;
                if (callback) {
                    callback();
                }
                return result;
            }

            UIEventResult OnKeyEvent(const UIKeyEvent &) override
            {
                auto snapshot = state;
                ++snapshot->keyCount;
                auto       callback = snapshot->onKey;
                const auto result   = snapshot->keyResult;
                if (callback) {
                    callback();
                }
                return result;
            }

            UIEventResult OnTextInput(const UITextInputEvent &) override
            {
                auto snapshot = state;
                ++snapshot->textCount;
                auto       callback = snapshot->onText;
                const auto result   = snapshot->textResult;
                if (callback) {
                    callback();
                }
                return result;
            }

            void OnPointerEnter(const UIPointerEvent &) override
            {
                auto snapshot = state;
                ++snapshot->enterCount;
                auto callback = snapshot->onEnter;
                if (callback) {
                    callback();
                }
            }

            void OnPointerLeave(const UIPointerEvent &) override
            {
                auto snapshot = state;
                ++snapshot->leaveCount;
                auto callback = snapshot->onLeave;
                if (callback) {
                    callback();
                }
            }

            void OnPointerCancel(uint32_t pointerId) override
            {
                auto snapshot = state;
                snapshot->cancellations.push_back(pointerId);
                auto callback = snapshot->onCancel;
                if (callback) {
                    callback();
                }
            }

            bool WantsPointerCapture() const override
            {
                auto snapshot = state;
                ++snapshot->captureQueryCount;
                auto       callback = snapshot->onCaptureQuery;
                const bool result   = snapshot->wantsCapture;
                if (callback) {
                    callback();
                }
                return result;
            }

        private:
            std::shared_ptr<InputLifetimeState> state;
        };

        InputLifetimeProbe *AddProbe(UIElement &parent, const std::shared_ptr<InputLifetimeState> &state, const UIRect &bounds)
        {
            auto  holder = std::make_unique<InputLifetimeProbe>(state);
            auto *probe  = holder.get();
            probe->SetBounds(bounds);
            parent.AddChild(std::move(holder));
            return probe;
        }

        UIPointerEvent Pointer(UIPointerAction action, float x = 10.0f, uint32_t pointerId = 0)
        {
            UIPointerEvent event;
            event.action    = action;
            event.pointerId = pointerId;
            event.x         = x;
            event.y         = 10.0f;
            return event;
        }

        template <typename Container>
        void CheckDirectCaptureDetach()
        {
            Container panel;
            panel.SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
            auto  state = std::make_shared<InputLifetimeState>();
            auto *leaf  = AddProbe(panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
            ASSERT_EQ(panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 7)), UIEventResult::HANDLED);

            auto detached = panel.RemoveChild(leaf);

            EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{7}));
            EXPECT_EQ(state->destroyCount, 0);
            detached.reset();

            auto  holder = std::make_unique<Button>();
            auto *button = holder.get();
            button->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
            int clicks = 0;
            button->SetOnClick([&] { ++clicks; });
            panel.AddChild(std::move(holder));
            panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 7));
            ASSERT_EQ(button->GetState(), Button::State::Pressed);
            auto pressed = panel.RemoveChild(button);
            EXPECT_NE(button->GetState(), Button::State::Pressed);
            panel.AddChild(std::move(pressed));
            panel.RoutePointer(Pointer(UIPointerAction::UP, 10.0f, 7));
            EXPECT_EQ(clicks, 0);
            panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 7));
            panel.RoutePointer(Pointer(UIPointerAction::UP, 10.0f, 7));
            EXPECT_EQ(clicks, 1);
        }

        template <typename Container>
        void CheckDirectUpDestroysContainer()
        {
            auto panel = std::make_unique<Container>();
            panel->SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
            auto state = std::make_shared<InputLifetimeState>();
            AddProbe(*panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
            ASSERT_EQ(panel->RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 7)), UIEventResult::HANDLED);
            state->onPointer = [&] { panel.reset(); };

            EXPECT_EQ(panel->RoutePointer(Pointer(UIPointerAction::UP, 10.0f, 7)), UIEventResult::HANDLED);

            EXPECT_EQ(panel, nullptr);
            EXPECT_EQ(state->destroyCount, 1);
            EXPECT_TRUE(state->cancellations.empty()); // UP released capture before user code.
        }

        template <typename Container>
        void CheckDirectCapturePointerIsolation()
        {
            Container panel;
            panel.SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
            auto state = std::make_shared<InputLifetimeState>();
            AddProbe(panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
            ASSERT_EQ(panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 22)), UIEventResult::HANDLED);

            panel.OnPointerCancel(11);
            EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::UP, 10.0f, 11)), UIEventResult::UNHANDLED);
            EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 11)), UIEventResult::UNHANDLED);
            EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::MOVE, 10.0f, 11)), UIEventResult::UNHANDLED);
            EXPECT_TRUE(state->cancellations.empty());
            EXPECT_EQ(state->deliveredPointerIds, (std::vector<uint32_t>{22}));

            EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::UP, 180.0f, 22)), UIEventResult::HANDLED);
            EXPECT_EQ(state->deliveredPointerIds, (std::vector<uint32_t>{22, 22}));
            panel.RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 22));
            panel.OnPointerCancel(22);
            EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{22}));
        }

    } // namespace

    TEST(UIInputLifetimeTest, DetachClearsFocusHoverAndCaptureBeforeOneCancellation)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));
        ASSERT_EQ(router.GetFocus(), leaf);
        ASSERT_EQ(router.GetHovered(), leaf);
        ASSERT_EQ(router.GetCapture(), leaf);

        state->onCancel = [&] {
            EXPECT_EQ(router.GetFocus(), nullptr);
            EXPECT_EQ(router.GetHovered(), nullptr);
            EXPECT_EQ(router.GetCapture(), nullptr);
        };
        auto detached = context.GetRoot()->RemoveChild(leaf);

        ASSERT_NE(detached, nullptr);
        EXPECT_EQ(detached->GetParent(), nullptr);
        EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{0}));
        EXPECT_EQ(state->leaveCount, 0); // Removal has no valid surface leave coordinate.
        EXPECT_EQ(router.DispatchKey({}), UIEventResult::UNHANDLED);
        EXPECT_EQ(router.DispatchText({"obsolete"}), UIEventResult::UNHANDLED);
        EXPECT_EQ(state->keyCount, 0);
        EXPECT_EQ(state->textCount, 0);
    }

    TEST(UIInputLifetimeTest, DetachPreservesUnrelatedFocusHoverAndOtherPointerCapture)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          removedState = std::make_shared<InputLifetimeState>();
        auto          siblingState = std::make_shared<InputLifetimeState>();
        auto         *removed      = AddProbe(*context.GetRoot(), removedState, {0.0f, 0.0f, 50.0f, 50.0f});
        auto         *sibling      = AddProbe(*context.GetRoot(), siblingState, {100.0f, 0.0f, 150.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 110.0f));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));

        auto detached = context.GetRoot()->RemoveChild(removed);

        EXPECT_EQ(router.GetFocus(), sibling);
        EXPECT_EQ(router.GetHovered(), sibling);
        EXPECT_EQ(router.GetCapture(1), nullptr);
        EXPECT_EQ(router.GetCapture(2), sibling);
        EXPECT_EQ(removedState->cancellations, (std::vector<uint32_t>{1}));
        EXPECT_TRUE(siblingState->cancellations.empty());
        EXPECT_EQ(router.DispatchText({"live"}), UIEventResult::HANDLED);
        EXPECT_EQ(siblingState->textCount, 1);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE, 180.0f, 2));
        EXPECT_EQ(siblingState->pointerCount, 3); // Initial hover, press, captured move.
    }

    TEST(UIInputLifetimeTest, RemovingAncestorClearsDescendantTargetsForEveryPointer)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto *branch = context.AddChild(std::make_unique<UIElement>());
        branch->SetBounds({0.0f, 0.0f, 100.0f, 100.0f});
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*branch, state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 2));
        ASSERT_EQ(router.GetFocus(), leaf);

        auto detached = context.GetRoot()->RemoveChild(branch);

        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetHovered(), nullptr);
        EXPECT_EQ(router.GetCapture(1), nullptr);
        EXPECT_EQ(router.GetCapture(2), nullptr);
        std::sort(state->cancellations.begin(), state->cancellations.end());
        EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{0, 1, 2}));
        detached.reset();
        EXPECT_EQ(state->destroyCount, 1);
        EXPECT_EQ(router.DispatchKey({}), UIEventResult::UNHANDLED);
    }

    TEST(UIInputLifetimeTest, ClearChildrenCancelsBeforeDestructionAndPreservesCallbackAdditions)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          state            = std::make_shared<InputLifetimeState>();
        auto          replacementState = std::make_shared<InputLifetimeState>();
        auto         *leaf             = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(context);
        router.SetFocus(leaf);
        InputLifetimeProbe *replacement = nullptr;
        state->onCancel                 = [&] {
            EXPECT_EQ(state->destroyCount, 0);
            EXPECT_EQ(router.GetFocus(), nullptr);
            replacement = AddProbe(*context.GetRoot(), replacementState, {100.0f, 0.0f, 150.0f, 50.0f});
        };

        context.GetRoot()->ClearChildren();

        EXPECT_EQ(state->destroyCount, 1);
        ASSERT_EQ(context.GetRoot()->GetChildren().size(), 1u);
        EXPECT_EQ(context.GetRoot()->GetChildren().front().get(), replacement);
        EXPECT_EQ(router.HitTest(110.0f, 10.0f), replacement);
    }

    TEST(UIInputLifetimeTest, SameContextReattachDoesNotRestorePressedButtonOrInputState)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  holder = std::make_unique<Button>();
        auto *button = holder.get();
        button->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        button->SetFocusable(true);
        int clicks = 0;
        button->SetOnClick([&] { ++clicks; });
        context.AddChild(std::move(holder));
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));
        ASSERT_EQ(button->GetState(), Button::State::Pressed);

        auto detached = context.GetRoot()->RemoveChild(button);
        context.AddChild(std::move(detached));
        router.DispatchPointer(Pointer(UIPointerAction::UP));

        EXPECT_EQ(clicks, 0);
        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetCapture(), nullptr);
        EXPECT_NE(button->GetState(), Button::State::Pressed);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));
        router.DispatchPointer(Pointer(UIPointerAction::UP));
        EXPECT_EQ(clicks, 1);
    }

    TEST(UIInputLifetimeTest, SiblingReorderPreservesTheCurrentAttachment)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  state        = std::make_shared<InputLifetimeState>();
        auto  siblingState = std::make_shared<InputLifetimeState>();
        auto *leaf         = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        AddProbe(*context.GetRoot(), siblingState, {100.0f, 0.0f, 150.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));

        ASSERT_TRUE(context.GetRoot()->MoveChild(leaf, 1));

        EXPECT_EQ(router.GetFocus(), leaf);
        EXPECT_EQ(router.GetHovered(), leaf);
        EXPECT_EQ(router.GetCapture(), leaf);
        EXPECT_TRUE(state->cancellations.empty());
        EXPECT_EQ(router.DispatchKey({}), UIEventResult::HANDLED);
    }

    TEST(UIInputLifetimeTest, CrossContextTransferRequiresNewFocusAndCapture)
    {
        UIContext source;
        UIContext destination;
        source.SetContentSize(200.0f, 100.0f);
        destination.SetContentSize(200.0f, 100.0f);
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*source.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter sourceRouter(source);
        UIEventRouter destinationRouter(destination);
        sourceRouter.DispatchPointer(Pointer(UIPointerAction::DOWN));

        destination.AddChild(source.GetRoot()->RemoveChild(leaf));

        EXPECT_EQ(sourceRouter.GetFocus(), nullptr);
        EXPECT_EQ(sourceRouter.GetCapture(), nullptr);
        EXPECT_EQ(destinationRouter.GetFocus(), nullptr);
        EXPECT_EQ(destinationRouter.GetCapture(), nullptr);
        EXPECT_EQ(sourceRouter.DispatchText({"old"}), UIEventResult::UNHANDLED);
        destinationRouter.DispatchPointer(Pointer(UIPointerAction::DOWN));
        EXPECT_EQ(destinationRouter.GetFocus(), leaf);
        EXPECT_EQ(destinationRouter.DispatchText({"new"}), UIEventResult::HANDLED);
        EXPECT_EQ(state->textCount, 1);
    }

    TEST(UIInputLifetimeTest, FocusRejectsForeignAndDetachedElements)
    {
        UIContext     context;
        UIContext     foreignContext;
        auto          state        = std::make_shared<InputLifetimeState>();
        auto          foreignState = std::make_shared<InputLifetimeState>();
        auto         *local        = AddProbe(*context.GetRoot(), state, {});
        auto         *foreign      = AddProbe(*foreignContext.GetRoot(), foreignState, {});
        auto          detached     = std::make_unique<InputLifetimeProbe>(std::make_shared<InputLifetimeState>());
        UIEventRouter router(context);

        router.SetFocus(foreign);
        EXPECT_EQ(router.GetFocus(), nullptr);
        router.SetFocus(detached.get());
        EXPECT_EQ(router.GetFocus(), nullptr);
        router.SetFocus(local);
        EXPECT_EQ(router.GetFocus(), local);
        router.SetFocus(nullptr);
        EXPECT_EQ(router.GetFocus(), nullptr);
    }

    TEST(UIInputLifetimeTest, EveryRouterAttachedToContextObservesRemoval)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter first(context);
        UIEventRouter second(context);
        first.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        second.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        second.DispatchPointer(Pointer(UIPointerAction::MOVE));
        second.SetFocus(leaf);
        state->onCancel = [&] {
            EXPECT_EQ(first.GetFocus(), nullptr);
            EXPECT_EQ(first.GetCapture(1), nullptr);
            EXPECT_EQ(second.GetFocus(), nullptr);
            EXPECT_EQ(second.GetHovered(), nullptr);
            EXPECT_EQ(second.GetCapture(1), nullptr);
        };

        auto detached = context.GetRoot()->RemoveChild(leaf);

        EXPECT_EQ(first.GetFocus(), nullptr);
        EXPECT_EQ(first.GetCapture(1), nullptr);
        EXPECT_EQ(second.GetFocus(), nullptr);
        EXPECT_EQ(second.GetHovered(), nullptr);
        EXPECT_EQ(second.GetCapture(1), nullptr);
        std::sort(state->cancellations.begin(), state->cancellations.end());
        EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{0, 1}));
        detached.reset();
        EXPECT_EQ(first.DispatchKey({}), UIEventResult::UNHANDLED);
        EXPECT_EQ(second.DispatchText({"removed"}), UIEventResult::UNHANDLED);
    }

    TEST(UIInputLifetimeTest, ContextMayBeDestroyedBeforeItsRouter)
    {
        auto context = std::make_unique<UIContext>();
        context->SetContentSize(200.0f, 100.0f);
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*context->GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(*context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));
        ASSERT_EQ(router.GetFocus(), leaf);
        state->onCancel = [&] {
            EXPECT_EQ(router.GetFocus(), nullptr);
            EXPECT_EQ(router.GetHovered(), nullptr);
            EXPECT_EQ(router.GetCapture(), nullptr);
        };

        context.reset();

        EXPECT_EQ(state->destroyCount, 1);
        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetHovered(), nullptr);
        EXPECT_EQ(router.GetCapture(), nullptr);
        EXPECT_EQ(router.HitTest(10.0f, 10.0f), nullptr);
        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::DOWN)), UIEventResult::UNHANDLED);
        EXPECT_EQ(router.DispatchKey({}), UIEventResult::UNHANDLED);
        EXPECT_EQ(router.DispatchText({"closed"}), UIEventResult::UNHANDLED);
        router.Reset();
    }

    TEST(UIInputLifetimeTest, DestroyedRouterLeavesNoContextNotification)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  state = std::make_shared<InputLifetimeState>();
        auto *leaf  = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        {
            UIEventRouter router(context);
            router.DispatchPointer(Pointer(UIPointerAction::DOWN));
        }
        const auto cancellationsAfterRouterDestruction = state->cancellations;
        auto       detached                            = context.GetRoot()->RemoveChild(leaf);
        EXPECT_EQ(state->cancellations, cancellationsAfterRouterDestruction);
    }

    TEST(UIInputLifetimeTest, PointerCallbackCanDestroyItsAncestorWithoutBubblingThroughIt)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  parentState = std::make_shared<InputLifetimeState>();
        auto  childState  = std::make_shared<InputLifetimeState>();
        auto *parent      = AddProbe(*context.GetRoot(), parentState, {0.0f, 0.0f, 100.0f, 100.0f});
        AddProbe(*parent, childState, {0.0f, 0.0f, 50.0f, 50.0f});
        childState->pointerResult = UIEventResult::UNHANDLED;
        childState->onPointer     = [&] { context.GetRoot()->RemoveChild(parent).reset(); };
        UIEventRouter router(context);

        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::DOWN)), UIEventResult::UNHANDLED);

        EXPECT_EQ(childState->pointerCount, 1);
        EXPECT_EQ(parentState->pointerCount, 0);
        EXPECT_EQ(childState->destroyCount, 1);
        EXPECT_EQ(parentState->destroyCount, 1);
        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetCapture(), nullptr);
    }

    TEST(UIInputLifetimeTest, KeyAndTextCallbacksCanDestroyTheirAncestor)
    {
        for (bool useText : {false, true}) {
            SCOPED_TRACE(useText ? "text" : "key");
            UIContext context;
            auto      parentState  = std::make_shared<InputLifetimeState>();
            auto      childState   = std::make_shared<InputLifetimeState>();
            auto     *parent       = AddProbe(*context.GetRoot(), parentState, {});
            auto     *leaf         = AddProbe(*parent, childState, {});
            childState->keyResult  = UIEventResult::UNHANDLED;
            childState->textResult = UIEventResult::UNHANDLED;
            auto remove            = [&] { context.GetRoot()->RemoveChild(parent).reset(); };
            childState->onKey      = remove;
            childState->onText     = remove;
            UIEventRouter router(context);
            router.SetFocus(leaf);

            const auto result = useText ? router.DispatchText({"destroy"}) : router.DispatchKey({});

            EXPECT_EQ(result, UIEventResult::UNHANDLED);
            EXPECT_EQ(childState->destroyCount, 1);
            EXPECT_EQ(parentState->destroyCount, 1);
            EXPECT_EQ(parentState->keyCount, 0);
            EXPECT_EQ(parentState->textCount, 0);
            EXPECT_EQ(router.GetFocus(), nullptr);
        }
    }

    TEST(UIInputLifetimeTest, EnterCallbackCanDestroyHitBeforePointerDelivery)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  state    = std::make_shared<InputLifetimeState>();
        auto *leaf     = AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        state->onEnter = [&] { context.GetRoot()->RemoveChild(leaf).reset(); };
        UIEventRouter router(context);

        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::MOVE)), UIEventResult::UNHANDLED);

        EXPECT_EQ(state->enterCount, 1);
        EXPECT_EQ(state->pointerCount, 0);
        EXPECT_EQ(state->destroyCount, 1);
        EXPECT_EQ(router.GetHovered(), nullptr);
    }

    TEST(UIInputLifetimeTest, LeaveCallbackCanDestroyTheNextHitBeforeEnter)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto firstState = std::make_shared<InputLifetimeState>();
        auto nextState  = std::make_shared<InputLifetimeState>();
        AddProbe(*context.GetRoot(), firstState, {0.0f, 0.0f, 50.0f, 50.0f});
        auto         *next = AddProbe(*context.GetRoot(), nextState, {100.0f, 0.0f, 150.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        firstState->onLeave = [&] { context.GetRoot()->RemoveChild(next).reset(); };

        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::MOVE, 110.0f)), UIEventResult::UNHANDLED);

        EXPECT_EQ(firstState->leaveCount, 1);
        EXPECT_EQ(nextState->destroyCount, 1);
        EXPECT_EQ(nextState->enterCount, 0);
        EXPECT_EQ(nextState->pointerCount, 0);
        EXPECT_EQ(router.GetHovered(), nullptr);
    }

    TEST(UIInputLifetimeTest, ReattachDuringPointerCallbackStopsTheOldBubblePath)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  oldParentState     = std::make_shared<InputLifetimeState>();
        auto  newParentState     = std::make_shared<InputLifetimeState>();
        auto  leafState          = std::make_shared<InputLifetimeState>();
        auto *oldParent          = AddProbe(*context.GetRoot(), oldParentState, {0.0f, 0.0f, 100.0f, 100.0f});
        auto *newParent          = AddProbe(*context.GetRoot(), newParentState, {100.0f, 0.0f, 150.0f, 50.0f});
        auto *leaf               = AddProbe(*oldParent, leafState, {0.0f, 0.0f, 50.0f, 50.0f});
        leafState->pointerResult = UIEventResult::UNHANDLED;
        leafState->onPointer     = [&] { newParent->AddChild(oldParent->RemoveChild(leaf)); };
        UIEventRouter router(context);

        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::DOWN)), UIEventResult::UNHANDLED);

        EXPECT_EQ(leaf->GetParent(), newParent);
        EXPECT_EQ(oldParentState->pointerCount, 0);
        EXPECT_EQ(newParentState->pointerCount, 0);
        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetCapture(), nullptr);
    }

    TEST(UIInputLifetimeTest, CancellationCanRemoveAnotherCapturedSubtree)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto          firstState = std::make_shared<InputLifetimeState>();
        auto          nextState  = std::make_shared<InputLifetimeState>();
        auto         *first      = AddProbe(*context.GetRoot(), firstState, {0.0f, 0.0f, 50.0f, 50.0f});
        auto         *next       = AddProbe(*context.GetRoot(), nextState, {100.0f, 0.0f, 150.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));
        firstState->onCancel = [&] { context.GetRoot()->RemoveChild(next).reset(); };

        auto detached = context.GetRoot()->RemoveChild(first);

        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetCapture(1), nullptr);
        EXPECT_EQ(router.GetCapture(2), nullptr);
        EXPECT_EQ(firstState->cancellations, (std::vector<uint32_t>{1}));
        std::sort(nextState->cancellations.begin(), nextState->cancellations.end());
        EXPECT_EQ(nextState->cancellations, (std::vector<uint32_t>{0, 2}));
        EXPECT_EQ(nextState->destroyCount, 1);
    }

    TEST(UIInputLifetimeTest, ResetRemainsSilentAndClearsAllRoutes)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto state = std::make_shared<InputLifetimeState>();
        AddProbe(*context.GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN));

        router.Reset();

        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetHovered(), nullptr);
        EXPECT_EQ(router.GetCapture(), nullptr);
        EXPECT_TRUE(state->cancellations.empty());
        EXPECT_EQ(state->leaveCount, 0);
    }

    TEST(UIInputLifetimeTest, StackPanelDetachCancelsTheOriginalDirectPointer)
    {
        CheckDirectCaptureDetach<StackPanel>();
    }

    TEST(UIInputLifetimeTest, DockPanelDetachCancelsTheOriginalDirectPointer)
    {
        CheckDirectCaptureDetach<DockPanel>();
    }

    TEST(UIInputLifetimeTest, GridPanelDetachCancelsTheOriginalDirectPointer)
    {
        CheckDirectCaptureDetach<GridPanel>();
    }

    TEST(UIInputLifetimeTest, OverlayDetachCancelsTheOriginalDirectPointer)
    {
        CheckDirectCaptureDetach<Overlay>();
    }

    TEST(UIInputLifetimeTest, DirectUpCanDestroyEveryNativeContainer)
    {
        CheckDirectUpDestroysContainer<StackPanel>();
        CheckDirectUpDestroysContainer<DockPanel>();
        CheckDirectUpDestroysContainer<GridPanel>();
        CheckDirectUpDestroysContainer<Overlay>();
    }

    TEST(UIInputLifetimeTest, CaptureQueryCanDestroyItsOwnElement)
    {
        Overlay panel;
        panel.SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
        auto  state           = std::make_shared<InputLifetimeState>();
        auto *leaf            = AddProbe(panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
        state->onCaptureQuery = [&] { panel.RemoveChild(leaf).reset(); };

        EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::DOWN)), UIEventResult::UNHANDLED);

        EXPECT_EQ(state->captureQueryCount, 1);
        EXPECT_EQ(state->pointerCount, 0);
        EXPECT_EQ(state->destroyCount, 1);
    }

    TEST(UIInputLifetimeTest, CaptureQueryCanDestroyItsContainer)
    {
        auto panel = std::make_unique<Overlay>();
        panel->SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
        auto state = std::make_shared<InputLifetimeState>();
        AddProbe(*panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
        state->onCaptureQuery = [&] { panel.reset(); };

        EXPECT_EQ(panel->RoutePointer(Pointer(UIPointerAction::DOWN)), UIEventResult::UNHANDLED);

        EXPECT_EQ(panel, nullptr);
        EXPECT_EQ(state->captureQueryCount, 1);
        EXPECT_EQ(state->pointerCount, 0);
        EXPECT_EQ(state->destroyCount, 1);
    }

    TEST(UIInputLifetimeTest, CaptureQueryCanRemoveAnotherChildFromThePaintSnapshot)
    {
        Overlay panel;
        panel.SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
        auto hitState     = std::make_shared<InputLifetimeState>();
        auto pendingState = std::make_shared<InputLifetimeState>();
        auto topState     = std::make_shared<InputLifetimeState>();
        AddProbe(panel, hitState, {0.0f, 0.0f, 50.0f, 50.0f});
        auto *pending = AddProbe(panel, pendingState, {100.0f, 0.0f, 150.0f, 50.0f});
        AddProbe(panel, topState, {170.0f, 0.0f, 190.0f, 50.0f});
        topState->onCaptureQuery = [&] { panel.RemoveChild(pending).reset(); };

        EXPECT_EQ(panel.RoutePointer(Pointer(UIPointerAction::DOWN)), UIEventResult::HANDLED);

        EXPECT_EQ(topState->captureQueryCount, 1);
        EXPECT_EQ(pendingState->destroyCount, 1);
        EXPECT_EQ(pendingState->captureQueryCount, 0);
        EXPECT_EQ(pendingState->pointerCount, 0);
        EXPECT_EQ(hitState->pointerCount, 1);
    }

    TEST(UIInputLifetimeTest, DirectCaptureIgnoresOtherPointersForEveryNativeContainer)
    {
        CheckDirectCapturePointerIsolation<StackPanel>();
        CheckDirectCapturePointerIsolation<DockPanel>();
        CheckDirectCapturePointerIsolation<GridPanel>();
        CheckDirectCapturePointerIsolation<Overlay>();
    }

    TEST(UIInputLifetimeTest, DirectAndRouterCaptureShareOneTargetPointerCancellation)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  holder = std::make_unique<Overlay>();
        auto *panel  = holder.get();
        panel->SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
        context.AddChild(std::move(holder));
        auto          state = std::make_shared<InputLifetimeState>();
        auto         *leaf  = AddProbe(*panel, state, {0.0f, 0.0f, 50.0f, 50.0f});
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::MOVE));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 7));
        panel->RoutePointer(Pointer(UIPointerAction::DOWN, 10.0f, 7));
        state->onCancel = [&] {
            EXPECT_EQ(router.GetFocus(), nullptr);
            EXPECT_EQ(router.GetHovered(), nullptr);
            EXPECT_EQ(router.GetCapture(7), nullptr);
        };

        auto detached = panel->RemoveChild(leaf);

        std::sort(state->cancellations.begin(), state->cancellations.end());
        EXPECT_EQ(state->cancellations, (std::vector<uint32_t>{0, 7}));
    }

    TEST(UIInputLifetimeTest, EventHandlersCanDestroyTheirRouter)
    {
        for (int kind = 0; kind < 3; ++kind) {
            SCOPED_TRACE(kind);
            UIContext context;
            context.SetContentSize(200.0f, 100.0f);
            auto  parentState = std::make_shared<InputLifetimeState>();
            auto  state       = std::make_shared<InputLifetimeState>();
            auto *parent      = AddProbe(*context.GetRoot(), parentState, {0.0f, 0.0f, 100.0f, 100.0f});
            auto *leaf        = AddProbe(*parent, state, {0.0f, 0.0f, 50.0f, 50.0f});
            auto  router      = std::make_unique<UIEventRouter>(context);
            router->SetFocus(leaf);
            state->pointerResult = UIEventResult::UNHANDLED;
            state->keyResult     = UIEventResult::UNHANDLED;
            state->textResult    = UIEventResult::UNHANDLED;
            auto destroyRouter   = [&] { router.reset(); };
            state->onPointer     = destroyRouter;
            state->onKey         = destroyRouter;
            state->onText        = destroyRouter;

            UIEventResult result;
            if (kind == 0) {
                result = router->DispatchPointer(Pointer(UIPointerAction::DOWN));
            } else if (kind == 1) {
                result = router->DispatchKey({});
            } else {
                result = router->DispatchText({"destroy router"});
            }

            EXPECT_EQ(result, UIEventResult::UNHANDLED);
            EXPECT_EQ(router, nullptr);
            EXPECT_EQ(state->destroyCount, 0);
            EXPECT_EQ(parentState->pointerCount, 0);
            EXPECT_EQ(parentState->keyCount, 0);
            EXPECT_EQ(parentState->textCount, 0);
            context.GetRoot()->ClearChildren();
            EXPECT_EQ(state->destroyCount, 1);
        }
    }

    TEST(UIInputLifetimeTest, EventHandlersCanDestroyTheirContext)
    {
        for (int kind = 0; kind < 3; ++kind) {
            SCOPED_TRACE(kind);
            auto context = std::make_unique<UIContext>();
            context->SetContentSize(200.0f, 100.0f);
            auto          state = std::make_shared<InputLifetimeState>();
            auto         *leaf  = AddProbe(*context->GetRoot(), state, {0.0f, 0.0f, 50.0f, 50.0f});
            UIEventRouter router(*context);
            router.SetFocus(leaf);
            state->pointerResult = UIEventResult::UNHANDLED;
            state->keyResult     = UIEventResult::UNHANDLED;
            state->textResult    = UIEventResult::UNHANDLED;
            auto destroyContext  = [&] { context.reset(); };
            state->onPointer     = destroyContext;
            state->onKey         = destroyContext;
            state->onText        = destroyContext;

            UIEventResult result;
            if (kind == 0) {
                result = router.DispatchPointer(Pointer(UIPointerAction::DOWN));
            } else if (kind == 1) {
                result = router.DispatchKey({});
            } else {
                result = router.DispatchText({"destroy context"});
            }

            EXPECT_EQ(result, UIEventResult::UNHANDLED);
            EXPECT_EQ(context, nullptr);
            EXPECT_EQ(state->destroyCount, 1);
            EXPECT_EQ(router.GetFocus(), nullptr);
            EXPECT_EQ(router.GetCapture(), nullptr);
        }
    }

    TEST(UIInputLifetimeTest, HoverCallbacksCanDestroyTheirContext)
    {
        for (bool duringLeave : {false, true}) {
            SCOPED_TRACE(duringLeave ? "leave" : "enter");
            auto context = std::make_unique<UIContext>();
            context->SetContentSize(200.0f, 100.0f);
            auto firstState = std::make_shared<InputLifetimeState>();
            auto nextState  = std::make_shared<InputLifetimeState>();
            AddProbe(*context->GetRoot(), firstState, {0.0f, 0.0f, 50.0f, 50.0f});
            AddProbe(*context->GetRoot(), nextState, {100.0f, 0.0f, 150.0f, 50.0f});
            UIEventRouter router(*context);
            if (duringLeave) {
                router.DispatchPointer(Pointer(UIPointerAction::MOVE));
                firstState->onLeave = [&] { context.reset(); };
            } else {
                firstState->onEnter = [&] { context.reset(); };
            }

            const auto result = router.DispatchPointer(Pointer(UIPointerAction::MOVE, duringLeave ? 110.0f : 10.0f));

            EXPECT_EQ(result, UIEventResult::UNHANDLED);
            EXPECT_EQ(context, nullptr);
            EXPECT_EQ(firstState->destroyCount, 1);
            EXPECT_EQ(nextState->destroyCount, 1);
            EXPECT_EQ(nextState->enterCount, 0);
            EXPECT_EQ(router.GetHovered(), nullptr);
        }
    }

    TEST(UIInputLifetimeTest, CancellationCanDestroyContextBeforePendingTargetsAreNotified)
    {
        auto context = std::make_unique<UIContext>();
        context->SetContentSize(200.0f, 100.0f);
        auto firstState = std::make_shared<InputLifetimeState>();
        auto nextState  = std::make_shared<InputLifetimeState>();
        AddProbe(*context->GetRoot(), firstState, {0.0f, 0.0f, 50.0f, 50.0f});
        AddProbe(*context->GetRoot(), nextState, {100.0f, 0.0f, 150.0f, 50.0f});
        UIEventRouter router(*context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));
        firstState->onCancel = [&] { context.reset(); };

        context->GetRoot()->ClearChildren();

        EXPECT_EQ(context, nullptr);
        EXPECT_EQ(firstState->destroyCount, 1);
        EXPECT_EQ(nextState->destroyCount, 1);
        EXPECT_EQ(router.GetFocus(), nullptr);
        EXPECT_EQ(router.GetCapture(1), nullptr);
        EXPECT_EQ(router.GetCapture(2), nullptr);
        std::sort(nextState->cancellations.begin(), nextState->cancellations.end());
        EXPECT_EQ(nextState->cancellations, (std::vector<uint32_t>{0, 2}));
    }

    TEST(UIInputLifetimeTest, CancellationCanDestroyRouterBeforePendingTargetsAreNotified)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto firstState = std::make_shared<InputLifetimeState>();
        auto nextState  = std::make_shared<InputLifetimeState>();
        AddProbe(*context.GetRoot(), firstState, {0.0f, 0.0f, 50.0f, 50.0f});
        AddProbe(*context.GetRoot(), nextState, {100.0f, 0.0f, 150.0f, 50.0f});
        auto router = std::make_unique<UIEventRouter>(context);
        router->DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
        router->DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));
        firstState->onCancel = [&] { router.reset(); };

        context.GetRoot()->ClearChildren();

        EXPECT_EQ(router, nullptr);
        EXPECT_EQ(firstState->destroyCount, 1);
        EXPECT_EQ(nextState->destroyCount, 1);
        std::sort(nextState->cancellations.begin(), nextState->cancellations.end());
        EXPECT_EQ(nextState->cancellations, (std::vector<uint32_t>{0, 2}));
    }

    TEST(UIInputLifetimeTest, CancellationTransferFlushesOldPressBeforeNewAttachment)
    {
        for (bool freshPress : {false, true}) {
            SCOPED_TRACE(freshPress ? "fresh press survives" : "old press is cleared");
            UIContext source;
            UIContext destination;
            source.SetContentSize(200.0f, 100.0f);
            destination.SetContentSize(200.0f, 100.0f);
            auto *branch = source.AddChild(std::make_unique<UIElement>());
            branch->SetBounds({0.0f, 0.0f, 200.0f, 100.0f});
            auto  firstState = std::make_shared<InputLifetimeState>();
            auto *first      = AddProbe(*branch, firstState, {0.0f, 0.0f, 50.0f, 50.0f});
            auto  holder     = std::make_unique<Button>();
            auto *button     = holder.get();
            button->SetBounds({100.0f, 0.0f, 150.0f, 50.0f});
            int clicks = 0;
            button->SetOnClick([&] { ++clicks; });
            branch->AddChild(std::move(holder));
            UIEventRouter sourceRouter(source);
            UIEventRouter destinationRouter(destination);
            sourceRouter.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 1));
            sourceRouter.DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));
            ASSERT_EQ(button->GetState(), Button::State::Pressed);
            sourceRouter.SetFocus(first); // Focus contributes the first cancellation in this batch.
            bool transferred     = false;
            firstState->onCancel = [&] {
                if (transferred) {
                    return;
                }
                transferred = true;
                destination.AddChild(branch->RemoveChild(button));
                EXPECT_NE(button->GetState(), Button::State::Pressed);
                if (freshPress) {
                    destinationRouter.DispatchPointer(Pointer(UIPointerAction::DOWN, 110.0f, 2));
                    EXPECT_EQ(button->GetState(), Button::State::Pressed);
                }
            };

            auto detached = source.GetRoot()->RemoveChild(branch);

            ASSERT_TRUE(transferred);
            EXPECT_EQ(sourceRouter.GetCapture(1), nullptr);
            EXPECT_EQ(sourceRouter.GetCapture(2), nullptr);
            if (freshPress) {
                EXPECT_EQ(button->GetState(), Button::State::Pressed);
            }
            destinationRouter.DispatchPointer(Pointer(UIPointerAction::UP, 110.0f, 2));
            EXPECT_EQ(clicks, freshPress ? 1 : 0);
        }
    }

    TEST(UIInputLifetimeTest, SingularCaptureCancellationStopsAfterTheTargetDetaches)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  parentState = std::make_shared<InputLifetimeState>();
        auto  leafState   = std::make_shared<InputLifetimeState>();
        auto *parent      = AddProbe(*context.GetRoot(), parentState, {0.0f, 0.0f, 100.0f, 100.0f});
        auto *leaf        = AddProbe(*parent, leafState, {0.0f, 0.0f, 50.0f, 50.0f});
        parent->SetFocusable(false);
        leaf->SetFocusable(false);
        UIEventRouter router(context);
        router.DispatchPointer(Pointer(UIPointerAction::DOWN, 10.0f, 7));
        ASSERT_EQ(router.GetCapture(7), leaf);
        ASSERT_EQ(router.GetFocus(), nullptr);
        ASSERT_EQ(router.GetHovered(), nullptr);
        UIElementPtr detached;
        leafState->onCancel = [&] {
            EXPECT_EQ(router.GetCapture(7), nullptr);
            detached = parent->RemoveChild(leaf);
        };
        UITransform singular;
        singular.scaleX = 0.0f;
        leaf->SetTransform(singular);

        EXPECT_EQ(router.DispatchPointer(Pointer(UIPointerAction::MOVE, 180.0f, 7)), UIEventResult::UNHANDLED);

        ASSERT_NE(detached, nullptr);
        EXPECT_EQ(detached.get(), leaf);
        EXPECT_EQ(leafState->destroyCount, 0);
        EXPECT_EQ(leafState->cancellations, (std::vector<uint32_t>{7}));
        EXPECT_TRUE(parentState->cancellations.empty());
        EXPECT_EQ(parentState->destroyCount, 0);
        EXPECT_EQ(router.GetCapture(7), nullptr);
    }

} // namespace sky::ui
