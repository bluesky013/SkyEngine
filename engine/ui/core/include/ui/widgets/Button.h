//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <functional>

namespace sky::ui {

    class Button : public UIElement {
    public:
        enum class State {
            Normal = 0,
            Hover,
            Pressed,
            Disabled,
        };

        Button()
        {
            SetLayoutVolatile(false);
        }
        ~Button() override = default;

        const char *GetTypeName() const override
        {
            return "Button";
        }

        void SetOnClick(std::function<void()> callback)
        {
            onClick = std::move(callback);
        }

        // Reflects the last interaction; returns Disabled whenever the element is disabled.
        State GetState() const
        {
            return IsEnabled() ? state : State::Disabled;
        }

        UIEventResult OnPointerEvent(const UIPointerEvent &event) override;
        void          OnPointerCancel(uint32_t pointerId) override;
        void          OnPaint(UIPaintContext &context) override;
        bool          SetProperty(const std::string &path, const UIPropertyValue &value) override;

    private:
        std::function<void()> onClick;
        State                 state   = State::Normal;
        bool                  pressed = false;
    };

} // namespace sky::ui
