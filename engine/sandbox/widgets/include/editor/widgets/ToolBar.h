//
// Created on 2026/10/07.
//

#pragma once

#include <editor/core/extension/EditorActionRegistry.h>
#include <ui/UIDrawData.h>
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

    // Engine-drawn toolbar: a horizontal strip of quick-action buttons, themed like
    // the rest of the chrome. Items carry a stable id so the host can toggle their
    // enabled state (Undo/Redo/Play/...). An item shows an icon (when a texture is
    // set) and/or its label.
    class ToolBar : public sky::ui::UIElement {
    public:
        struct Item {
            std::string                        id;
            std::string                        label;
            sky::ui::UITextureId               icon = sky::ui::UI_INVALID_TEXTURE;
            std::function<void()>              action;
            bool                               enabled         = true;
            bool                               separatorBefore = false;
            bool                               checkable       = false;
            bool                               checked         = false;
            bool                               visible         = true;
            std::function<EditorActionState()> resolveState;
        };

        explicit ToolBar(sky::ui::UITextSystem *text);
        ~ToolBar() override
        {
            lifetime.reset();
        }

        const char *GetTypeName() const override
        {
            return "ToolBar";
        }

        void SetItems(std::vector<Item> inItems);
        void SetItemEnabled(const std::string &id, bool enabled);
        void SetItemIcon(const std::string &id, sky::ui::UITextureId texture);
        bool RefreshActionStates();

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;

    private:
        float           ItemWidth(size_t index) const;
        sky::ui::UIRect ItemRect(size_t index) const;
        int             ItemAt(float x, float y) const;

        std::vector<Item>      items;
        sky::ui::UITextSystem *textSystem      = nullptr;
        int                    hoverItem       = -1;
        std::shared_ptr<void>  lifetime        = std::make_shared<int>(0);
        uint64_t               itemsGeneration = 0;
        bool                   refreshingState = false;
    };

} // namespace sky::editor
