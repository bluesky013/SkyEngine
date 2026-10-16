//
// Created on 2026/09/19.
//

#include <ui/UIPaintContext.h>
#include <ui/localization/UILocalization.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/Text.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::ui {

    namespace {

        float Padding(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }

        float OuterExtent(float content, float before, float after)
        {
            const double extent = static_cast<double>(content) + before + after;
            return static_cast<float>(std::min(extent, static_cast<double>(std::numeric_limits<float>::max())));
        }

    } // namespace

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
        // Localization can change through the shared table without a widget
        // setter. Keep keyed text live until that dependency has notifications.
        SetLayoutVolatile(!textKey.empty());
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
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    void Text::SetWrap(bool value)
    {
        if (wrap != value) {
            wrap = value;
            MarkLayoutDirty();
            MarkPaintDirty();
        }
    }

    UISize Text::OnMeasure(const UIConstraints &constraints)
    {
        const UILayoutParams &layout             = GetLayout();
        const float           left               = Padding(layout.paddingLeft);
        const float           top                = Padding(layout.paddingTop);
        const float           right              = Padding(layout.paddingRight);
        const float           bottom             = Padding(layout.paddingBottom);
        const UIConstraints   contentConstraints = constraints.Deflate(left, top, right, bottom);
        UITextExtent          extent;
        if (textSystem != nullptr) {
            extent = UITextLayout::Build(GetResolvedText(), fontSize, textSystem->GetAtlas(), contentConstraints.maxSize.width, wrap).extent;
        }
        return {OuterExtent(extent.width, left, right), OuterExtent(extent.height, top, bottom)};
    }

    void Text::OnPaint(UIPaintContext &context)
    {
        const std::string resolved = GetResolvedText();
        if (textSystem == nullptr || resolved.empty()) {
            return;
        }

        const UIRect           contentRect  = GetLayout().ContentRect(GetBounds());
        const float            contentWidth = GetContentSize().width;
        const UITextLayoutData textLayout   = UITextLayout::Build(resolved, fontSize, textSystem->GetAtlas(), contentWidth, wrap);
        UITextLineAlign        lineAlign    = UITextLineAlign::LEFT;
        if (align == Align::CENTER) {
            lineAlign = UITextLineAlign::CENTER;
        } else if (align == Align::RIGHT) {
            lineAlign = UITextLineAlign::RIGHT;
        }

        UITextLayout::Emit(context, textLayout, contentRect.left, contentRect.top, textColor, textSystem->GetAtlas(), contentWidth, lineAlign);
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
        if (path == "text.wrap" && value.type == UIPropertyValue::Type::BOOL) {
            SetWrap(value.boolValue);
            return true;
        }
        return UIElement::SetProperty(path, value);
    }

} // namespace sky::ui
