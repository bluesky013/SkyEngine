//
// Created on 2026/10/07.
//

#pragma once

#include <ui/UIDrawData.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Engine-drawn toolbar: a horizontal strip of quick-action buttons, themed like
    // the rest of the chrome. Items carry a stable id so the host can toggle their
    // enabled state (Undo/Redo/Play/...). An item shows an icon (when a texture is
    // set) and/or its label.
    class ToolBar : public sky::ui::UIElement {
    public:
        struct Item {
            std::string           id;
            std::string           label;
            sky::ui::UITextureId  icon = sky::ui::UI_INVALID_TEXTURE;
            std::function<void()> action;
            bool                  enabled         = true;
            bool                  separatorBefore = false;
        };

        explicit ToolBar(sky::ui::UITextSystem *text);

        const char *GetTypeName() const override
        {
            return "ToolBar";
        }

        void SetItems(std::vector<Item> inItems);
        void SetItemEnabled(const std::string &id, bool enabled);
        void SetItemIcon(const std::string &id, sky::ui::UITextureId texture);

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    private:
        float           ItemWidth(size_t index) const;
        sky::ui::UIRect ItemRect(size_t index) const;
        int             ItemAt(float x, float y) const;

        std::vector<Item>      items;
        sky::ui::UITextSystem *textSystem = nullptr;
        int                    hoverItem  = -1;
    };

} // namespace sky::editor
