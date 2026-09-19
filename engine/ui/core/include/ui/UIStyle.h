//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::ui {

    struct UIStyle {
        enum Field : uint32_t {
            FIELD_BACKGROUND     = 1u << 0,
            FIELD_TEXT           = 1u << 1,
            FIELD_BORDER_COLOR   = 1u << 2,
            FIELD_BORDER_WIDTH   = 1u << 3,
            FIELD_CORNER         = 1u << 4,
            FIELD_BUTTON_COLORS  = 1u << 5,
        };

        uint32_t backgroundColor = 0x00000000;
        uint32_t textColor       = 0xFF000000;
        uint32_t borderColor     = 0x00000000;
        float borderWidth        = 0.0f;
        float cornerRadius       = 0.0f;

        uint32_t buttonNormal  = 0xFF3A3A3A;
        uint32_t buttonHover   = 0xFF505050;
        uint32_t buttonPressed = 0xFF202020;

        // Tracks which fields were explicitly set, for per-field cascade. A style
        // with no flags set is treated as a full override by UITheme::Resolve.
        uint32_t flags = 0;

        bool Has(uint32_t field) const { return (flags & field) != 0; }

        void SetBackgroundColor(uint32_t value)
        {
            backgroundColor = value;
            flags |= FIELD_BACKGROUND;
        }
        void SetTextColor(uint32_t value)
        {
            textColor = value;
            flags |= FIELD_TEXT;
        }
        void SetBorderColor(uint32_t value)
        {
            borderColor = value;
            flags |= FIELD_BORDER_COLOR;
        }
        void SetBorderWidth(float value)
        {
            borderWidth = value;
            flags |= FIELD_BORDER_WIDTH;
        }
        void SetCornerRadius(float value)
        {
            cornerRadius = value;
            flags |= FIELD_CORNER;
        }
        void SetButtonColors(uint32_t normal, uint32_t hover, uint32_t pressed)
        {
            buttonNormal = normal;
            buttonHover = hover;
            buttonPressed = pressed;
            flags |= FIELD_BUTTON_COLORS;
        }
    };

    class UITheme {
    public:
        void SetStyle(const std::string &className, const UIStyle &style);
        bool HasStyle(const std::string &className) const;

        // Merges classes in order. Flagged fields override the accumulator; a
        // class with no flags replaces the whole style.
        UIStyle Resolve(const std::vector<std::string> &classes) const;

    private:
        std::unordered_map<std::string, UIStyle> styles;
    };

} // namespace sky::ui
