//
// Created on 2026/09/19.
//

#include <ui/widgets/EditBox.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>

namespace sky::ui {

    namespace {

        constexpr uint32_t KEY_BACKSPACE = 0x08;
        constexpr uint32_t KEY_LEFT = 0x25;
        constexpr uint32_t KEY_RIGHT = 0x27;

    } // namespace

    EditBox::EditBox()
    {
        SetFocusable(true);
    }

    void EditBox::SetText(const std::string &value)
    {
        text = value;
        if (caret > text.size()) {
            caret = text.size();
        }
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void EditBox::SetCaret(size_t value)
    {
        caret = value > text.size() ? text.size() : value;
        MarkPaintDirty();
    }

    void EditBox::SetTextSystem(UITextSystem *value)
    {
        textSystem = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void EditBox::SetFontSize(uint32_t value)
    {
        fontSize = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void EditBox::SetTextColor(uint32_t value)
    {
        textColor = value;
        MarkPaintDirty();
    }

    UIEventResult EditBox::OnPointerEvent(const UIPointerEvent &event)
    {
        if (event.action == UIPointerAction::DOWN) {
            // Focus is assigned by the router; consume the press.
            return UIEventResult::HANDLED;
        }
        return UIEventResult::UNHANDLED;
    }

    UIEventResult EditBox::OnKeyEvent(const UIKeyEvent &event)
    {
        switch (event.keyCode) {
        case KEY_BACKSPACE:
            if (caret > 0) {
                text.erase(caret - 1, 1);
                caret--;
                MarkLayoutDirty();
                MarkPaintDirty();
            }
            return UIEventResult::HANDLED;
        case KEY_LEFT:
            if (caret > 0) {
                caret--;
                MarkPaintDirty();
            }
            return UIEventResult::HANDLED;
        case KEY_RIGHT:
            if (caret < text.size()) {
                caret++;
                MarkPaintDirty();
            }
            return UIEventResult::HANDLED;
        default:
            return UIEventResult::UNHANDLED;
        }
    }

    UIEventResult EditBox::OnTextInput(const UITextInputEvent &event)
    {
        if (event.text.empty()) {
            return UIEventResult::UNHANDLED;
        }
        text.insert(caret, event.text);
        caret += event.text.size();
        MarkLayoutDirty();
        MarkPaintDirty();
        return UIEventResult::HANDLED;
    }

    void EditBox::Measure(float &outWidth, float &outHeight)
    {
        outWidth = 0.0f;
        outHeight = 0.0f;
        if (textSystem == nullptr) {
            return;
        }
        const UITextExtent extent = UITextLayout::Measure(text, fontSize, textSystem->GetAtlas());
        outWidth = extent.width;
        outHeight = extent.height;
    }

    void EditBox::OnPaint(UIPaintContext &context)
    {
        const UITheme *theme = context.GetTheme();
        if (theme != nullptr) {
            const UIStyle style = theme->Resolve(GetStyleClasses());
            context.AddRect(GetBounds(), style.backgroundColor);
        }

        if (textSystem != nullptr && !text.empty()) {
            UITextLayout::Emit(context, text, fontSize, GetBounds().left, GetBounds().top, textColor, textSystem->GetAtlas());
        }
    }

    bool EditBox::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "text.content" && value.type == UIPropertyValue::Type::STRING) {
            SetText(value.stringValue);
            return true;
        }
        if (path == "text.size" && value.type == UIPropertyValue::Type::INT) {
            SetFontSize(static_cast<uint32_t>(value.intValue));
            return true;
        }
        if (path == "text.color" && value.type == UIPropertyValue::Type::INT) {
            SetTextColor(static_cast<uint32_t>(value.intValue));
            return true;
        }
        return UIElement::SetProperty(path, value);
    }

} // namespace sky::ui
