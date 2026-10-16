//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/extension/EditorActionRegistry.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <functional>
#include <memory>
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
            std::string                        label;
            std::function<void()>              action;
            std::vector<Item>                  children;          // non-empty => opens a second-level submenu
            std::string                        shortcut;          // optional accelerator hint, right-aligned
            std::string                        icon;              // optional icon name (IconRegistry)
            bool                               separator = false; // draws a divider instead of a row
            bool                               checkable = false; // reserves a check column; draws a checkbox
            bool                               checked   = false; // checked state (only when checkable)
            bool                               enabled   = true;
            bool                               visible   = true;
            std::function<EditorActionState()> resolveState;
        };
        struct Menu {
            std::string       label;
            std::vector<Item> items;
        };

        MenuBar(std::vector<Menu> inMenus, sky::ui::UITextSystem *text);
        ~MenuBar() override
        {
            lifetime.reset();
        }

        const char *GetTypeName() const override
        {
            return "MenuBar";
        }

        void SetBarHeight(float h)
        {
            barHeight = h;
        }
        // Replaces the menus (used when the chrome is rebuilt for a different action scope).
        void SetMenus(std::vector<Menu> inMenus, bool preserveOpen = false);
        bool RefreshActionStates();
        bool IsOpen() const
        {
            return openIndex >= 0;
        }
        float PopupHeight() const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    private:
        float           LabelWidth(const std::string &text) const;
        bool            MenuVisible(size_t index) const;
        static size_t   VisibleRowCount(const std::vector<Item> &items);
        float           RowHeight() const;
        sky::ui::UIRect LabelRect(size_t index) const;
        sky::ui::UIRect PopupRect() const;
        sky::ui::UIRect SubmenuRect() const;
        int32_t         LabelAt(float x, float y) const;
        int32_t         ItemAt(float x, float y) const;
        int32_t         SubmenuItemAt(float x, float y) const;
        void            PaintPopup(sky::ui::UIPaintContext &context,
                                   const std::vector<Item> &items,
                                   const sky::ui::UIRect   &popup,
                                   int32_t                  hovered,
                                   bool                     withSubmenuArrows) const;

        std::vector<Menu>      menus;
        sky::ui::UITextSystem *textSystem      = nullptr;
        int32_t                openIndex       = -1;
        int32_t                hoverLabel      = -1;
        int32_t                hoverItem       = -1;
        int32_t                hoverSubItem    = -1;
        float                  barHeight       = 24.0f;
        std::shared_ptr<void>  lifetime        = std::make_shared<int>(0);
        uint64_t               menusGeneration = 0;
        bool                   refreshingState = false;
    };

} // namespace sky::editor
