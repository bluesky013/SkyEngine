#pragma once

#include <framework/window/IWindowEvent.h>

namespace sky::editor {

    // Own one keyed subscription. Each proxy has a distinct listener identity,
    // so Event::DisConnect removes this window even when the owner hosts several.
    // The owner must destroy its bindings before destroying the event target.
    class WindowEventBinding final : public sky::IWindowEvent {
    public:
        WindowEventBinding(sky::IWindowEvent &target, const sky::NativeWindow *window) : target(target)
        {
            sky::Event<sky::IWindowEvent>::Connect(window, this);
        }

        ~WindowEventBinding() override
        {
            sky::Event<sky::IWindowEvent>::DisConnect(this);
        }

        WindowEventBinding(const WindowEventBinding &)            = delete;
        WindowEventBinding &operator=(const WindowEventBinding &) = delete;

        void OnWindowClose(const sky::NativeWindow *window) override
        {
            target.OnWindowClose(window);
        }
        void OnWindowMove(const sky::WindowMoveEvent &event) override
        {
            target.OnWindowMove(event);
        }
        void OnWindowResize(const sky::WindowResizeEvent &event) override
        {
            target.OnWindowResize(event);
        }

    private:
        sky::IWindowEvent &target;
    };

} // namespace sky::editor
