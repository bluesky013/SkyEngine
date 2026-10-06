//
// Created on 2026/10/06.
//

#pragma once

#include <ui/UIRect.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Engine-drawn menu bar: labeled top-level menus with a single open popup.
    // Reusable across the editor chrome.
    class MenuBar : public sky::ui::UIElement {
    public:
        struct Item {
            std::string           label;
            std::function<void()> action;
        };
        struct Menu {
            std::string       label;
            std::vector<Item> items;
        };

        MenuBar(std::vector<Menu> inMenus, sky::ui::UITextSystem *text);

        const char *GetTypeName() const override { return "MenuBar"; }

        void  SetBarHeight(float h) { barHeight = h; }
        bool  IsOpen() const { return openIndex >= 0; }
        float PopupHeight() const;

        void                       OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult     OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    private:
        static float        LabelWidth(const std::string &text);
        sky::ui::UIRect     LabelRect(size_t index) const;
        sky::ui::UIRect     PopupRect() const;
        int32_t             LabelAt(float x, float y) const;
        int32_t             ItemAt(float x, float y) const;

        std::vector<Menu>      menus;
        sky::ui::UITextSystem *textSystem = nullptr;
        int32_t                openIndex = -1;
        int32_t                hoverLabel = -1;
        int32_t                hoverItem = -1;
        float                  barHeight = 24.0f;
        const float            kPad = 2.0f;
        const float            kRow = 22.0f;
    };

} // namespace sky::editor
