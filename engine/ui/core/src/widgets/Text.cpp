//
// Created on 2026/09/19.
//

#include <ui/widgets/Text.h>
#include <ui/localization/UILocalization.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/UIPaintContext.h>

namespace sky::ui {

    void Text::SetTextSystem(UITextSystem *value)
    {
        textSystem = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void Text::SetContent(const std::string &value)
    {
        content = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void Text::SetTextKey(const std::string &value)
    {
        textKey = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    std::string Text::GetResolvedText() const
    {
        if (!textKey.empty()) {
            return UILocalization::Get()->Translate(textKey);
        }
        return content;
    }

    void Text::SetFontSize(uint32_t value)
    {
        fontSize = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void Text::SetTextColor(uint32_t value)
    {
        textColor = value;
        MarkPaintDirty();
    }

    void Text::SetAlign(Align value)
    {
        align = value;
        MarkPaintDirty();
    }

    void Text::Measure(float &outWidth, float &outHeight)
    {
        outWidth = 0.0f;
        outHeight = 0.0f;
        if (textSystem == nullptr) {
            return;
        }

        const UITextExtent extent = UITextLayout::Measure(GetResolvedText(), fontSize, textSystem->GetAtlas());
        outWidth = extent.width;
        outHeight = extent.height;
    }

    void Text::OnPaint(UIPaintContext &context)
    {
        const std::string resolved = GetResolvedText();
        if (textSystem == nullptr || resolved.empty()) {
            return;
        }

        const UITextExtent extent = UITextLayout::Measure(resolved, fontSize, textSystem->GetAtlas());
        float x = GetBounds().left;
        if (align == Align::CENTER) {
            x = GetBounds().left + (GetBounds().Width() - extent.width) * 0.5f;
        } else if (align == Align::RIGHT) {
            x = GetBounds().right - extent.width;
        }

        UITextLayout::Emit(context, resolved, fontSize, x, GetBounds().top, textColor, textSystem->GetAtlas());
    }

    bool Text::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "text.content" && value.type == UIPropertyValue::Type::STRING) {
            SetContent(value.stringValue);
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
