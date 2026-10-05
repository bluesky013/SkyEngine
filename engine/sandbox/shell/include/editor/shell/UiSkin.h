//
// Created on 2026/10/04.
//

#pragma once

#include <editor/shell/UiTheme.h>
#include <ui/UIRect.h>
#include <cstdint>
#include <string>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    enum class RowState { Normal, Alt, Hover, Selected };

    // Theme-driven component painter. Panels hold a skin and draw widgets with it
    // so the whole editor shares one look; supply a custom UiTheme to restyle.
    class UiSkin {
    public:
        UiSkin(const UiTheme &theme, sky::ui::UITextSystem *text)
            : theme(&theme)
            , textSystem(text)
        {
        }

        const UiTheme &Theme() const { return *theme; }
        sky::ui::UITextSystem *Text() const { return textSystem; }

        // Panel frame with a gradient header and accent underline; returns the
        // content rect below the header.
        sky::ui::UIRect DrawPanel(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds,
                                  const std::string &title) const;

        void DrawSectionHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect,
                               const std::string &title) const;

        void DrawRow(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, RowState state) const;

        void DrawField(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool focused,
                       bool invalid) const;

        void DrawCheckbox(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool checked,
                          bool hovered) const;

        void DrawSlider(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float value, float minValue,
                        float maxValue) const;

        void DrawColorSwatch(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t argb) const;

        void DrawScrollbar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &track, float contentHeight,
                           float scrollTop) const;

        void DrawPopup(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect) const;

        void DrawPopupItem(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool hovered,
                           bool selected) const;

        void DrawTab(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, const std::string &title,
                     bool active) const;

        void DrawToolItem(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, const std::string &label,
                          bool hovered) const;

        void DrawTriangle(sky::ui::UIPaintContext &context, float x, float cy, bool down, uint32_t color) const;
        void DrawCheck(sky::ui::UIPaintContext &context, const sky::ui::UIRect &box, uint32_t color) const;

    private:
        const UiTheme        *theme = nullptr;
        sky::ui::UITextSystem *textSystem = nullptr;
    };

} // namespace sky::editor
