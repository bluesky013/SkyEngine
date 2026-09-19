//
// Created on 2026/09/19.
//

#include <ui/UIStyle.h>

namespace sky::ui {

    void UITheme::SetStyle(const std::string &className, const UIStyle &style)
    {
        styles[className] = style;
    }

    bool UITheme::HasStyle(const std::string &className) const
    {
        return styles.find(className) != styles.end();
    }

    UIStyle UITheme::Resolve(const std::vector<std::string> &classes) const
    {
        UIStyle result;
        for (const auto &className : classes) {
            const auto it = styles.find(className);
            if (it == styles.end()) {
                continue;
            }

            const UIStyle &style = it->second;
            if (style.flags == 0) {
                result = style;
                continue;
            }

            if (style.Has(UIStyle::FIELD_BACKGROUND)) {
                result.backgroundColor = style.backgroundColor;
            }
            if (style.Has(UIStyle::FIELD_TEXT)) {
                result.textColor = style.textColor;
            }
            if (style.Has(UIStyle::FIELD_BORDER_COLOR)) {
                result.borderColor = style.borderColor;
            }
            if (style.Has(UIStyle::FIELD_BORDER_WIDTH)) {
                result.borderWidth = style.borderWidth;
            }
            if (style.Has(UIStyle::FIELD_CORNER)) {
                result.cornerRadius = style.cornerRadius;
            }
            if (style.Has(UIStyle::FIELD_BUTTON_COLORS)) {
                result.buttonNormal = style.buttonNormal;
                result.buttonHover = style.buttonHover;
                result.buttonPressed = style.buttonPressed;
            }
        }
        return result;
    }

} // namespace sky::ui
