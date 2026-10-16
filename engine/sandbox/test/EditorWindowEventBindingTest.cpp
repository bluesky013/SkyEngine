#include "WindowEventBinding.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <vector>

namespace sky::editor {
    namespace {
        struct WindowEvents final : sky::IWindowEvent {
            std::vector<const NativeWindow *> closed;
            std::vector<WindowMoveEvent>      moved;
            std::vector<WindowResizeEvent>    resized;
            std::function<void()>             onClose;

            void OnWindowClose(const NativeWindow *window) override
            {
                closed.push_back(window);
                if (onClose) {
                    onClose();
                }
            }
            void OnWindowMove(const WindowMoveEvent &event) override
            {
                moved.push_back(event);
            }
            void OnWindowResize(const WindowResizeEvent &event) override
            {
                resized.push_back(event);
            }
        };

        // Events only compare these opaque keys; no OS window or RHI is used.
        struct WindowKey {
            const NativeWindow *Get() const
            {
                return reinterpret_cast<const NativeWindow *>(this);
            }
        };
    } // namespace

    TEST(EditorWindowEventBindingTest, DetachingOneWindowPreservesOtherWindowAndEventIdentity)
    {
        WindowKey    first, second;
        WindowEvents events;
        auto         firstBinding  = std::make_unique<WindowEventBinding>(events, first.Get());
        auto         secondBinding = std::make_unique<WindowEventBinding>(events, second.Get());

        Event<IWindowEvent>::BroadCast(first.Get(), &IWindowEvent::OnWindowMove, WindowMoveEvent{11, 21, 31});
        Event<IWindowEvent>::BroadCast(second.Get(), &IWindowEvent::OnWindowResize, WindowResizeEvent{12, 640, 480});
        ASSERT_EQ(events.moved.size(), 1u);
        EXPECT_EQ(events.moved.front().winID, 11u);
        EXPECT_EQ(events.moved.front().x, 21);
        EXPECT_EQ(events.moved.front().y, 31);
        ASSERT_EQ(events.resized.size(), 1u);
        EXPECT_EQ(events.resized.front().winID, 12u);
        EXPECT_EQ(events.resized.front().width, 640u);
        EXPECT_EQ(events.resized.front().height, 480u);

        firstBinding.reset();
        Event<IWindowEvent>::BroadCast(first.Get(), &IWindowEvent::OnWindowClose, first.Get());
        Event<IWindowEvent>::BroadCast(second.Get(), &IWindowEvent::OnWindowClose, second.Get());
        ASSERT_EQ(events.closed.size(), 1u);
        EXPECT_EQ(events.closed.front(), second.Get());
    }

    TEST(EditorWindowEventBindingTest, ClosingWindowCanDetachItsOwnSubscriptionDuringBroadcast)
    {
        WindowKey    first, second;
        WindowEvents events;
        auto         firstBinding  = std::make_unique<WindowEventBinding>(events, first.Get());
        auto         secondBinding = std::make_unique<WindowEventBinding>(events, second.Get());
        events.onClose             = [&firstBinding]() { firstBinding.reset(); };

        Event<IWindowEvent>::BroadCast(first.Get(), &IWindowEvent::OnWindowClose, first.Get());
        Event<IWindowEvent>::BroadCast(first.Get(), &IWindowEvent::OnWindowClose, first.Get());
        Event<IWindowEvent>::BroadCast(second.Get(), &IWindowEvent::OnWindowClose, second.Get());
        ASSERT_EQ(events.closed.size(), 2u);
        EXPECT_EQ(events.closed[0], first.Get());
        EXPECT_EQ(events.closed[1], second.Get());
    }

    TEST(EditorWindowEventBindingTest, AllWindowSubscriptionsAreGoneBeforeTargetDestruction)
    {
        WindowKey first, second;
        {
            WindowEvents events;
            auto         firstBinding  = std::make_unique<WindowEventBinding>(events, first.Get());
            auto         secondBinding = std::make_unique<WindowEventBinding>(events, second.Get());
        }
        // ASan detects any stale target invocation; this also guards against the
        // old single DisConnect(owner) leaving one of several keyed entries.
        Event<IWindowEvent>::BroadCast(first.Get(), &IWindowEvent::OnWindowClose, first.Get());
        Event<IWindowEvent>::BroadCast(second.Get(), &IWindowEvent::OnWindowClose, second.Get());
    }

} // namespace sky::editor
