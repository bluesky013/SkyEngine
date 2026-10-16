//
// ToolItemButton: a labeled toolbar button (UiSkin-drawn, self hover/pressed/click), suitable as a
// StackPanel child. Fills the gap that sky::ui::Button has no label and UiSkin::DrawToolItem has no
// interaction. See docs/editor/ui-standards.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <functional>
#include <string>

namespace sky::editor {

    class UiSkin;

    class ToolItemButton : public sky::ui::UIElement {
    public:
        ToolItemButton(UiSkin *skin, std::string label, std::function<void()> onClick);

        // When set, the button renders icon-only (label becomes a hover tooltip); otherwise it renders
        // the text label. `textureId` is a UI texture id (see IconRegistry).
        void SetIcon(uint32_t textureId)
        {
            icon    = textureId;
            hasIcon = true;
        }

        const char *GetTypeName() const override
        {
            return "ToolItemButton";
        }

        void                   Measure(float &outWidth, float &outHeight) override;
        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        void                   OnPointerCancel(uint32_t pointerId) override;
        bool                   WantsPointerCapture() const override
        {
            return pressed;
        }

    private:
        UiSkin               *skin = nullptr;
        std::string           label;
        std::function<void()> onClick;
        bool                  hovered = false;
        bool                  pressed = false;
        uint32_t              icon    = 0;
        bool                  hasIcon = false;
    };

} // namespace sky::editor
