//
// Created on 2026/09/19.
//

#include <ui/widgets/Button.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>

namespace sky::ui {

    UIEventResult Button::OnPointerEvent(const UIPointerEvent &event)
    {
        if (!IsEnabled()) {
            return UIEventResult::UNHANDLED;
        }

        switch (event.action) {
        case UIPointerAction::DOWN:
            pressed = true;
            state = State::Pressed;
            MarkPaintDirty();
            return UIEventResult::HANDLED;

        case UIPointerAction::UP: {
            const bool inside = pressed && GetBounds().Contains(event.x, event.y);
            pressed = false;
            state = inside ? State::Hover : State::Normal;
            MarkPaintDirty();
            if (inside && onClick) {
                onClick();
            }
            return UIEventResult::HANDLED;
        }

        case UIPointerAction::MOVE:
            state = pressed ? State::Pressed : State::Hover;
            MarkPaintDirty();
            return UIEventResult::HANDLED;

        case UIPointerAction::WHEEL:
        default:
            return UIEventResult::UNHANDLED;
        }
    }

    bool Button::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "button.interactable" && value.type == UIPropertyValue::Type::BOOL) {
            SetEnabled(value.boolValue);
            return true;
        }
        return UIElement::SetProperty(path, value);
    }

    void Button::OnPaint(UIPaintContext &context)
    {
        const UITheme *theme = context.GetTheme();
        if (theme == nullptr) {
            return;
        }

        const UIStyle style = theme->Resolve(GetStyleClasses());
        uint32_t color = style.buttonNormal;
        switch (GetState()) {
        case State::Hover:
            color = style.buttonHover;
            break;
        case State::Pressed:
            color = style.buttonPressed;
            break;
        case State::Normal:
        case State::Disabled:
        default:
            color = style.buttonNormal;
            break;
        }
        context.AddRect(GetBounds(), color);
    }

} // namespace sky::ui
