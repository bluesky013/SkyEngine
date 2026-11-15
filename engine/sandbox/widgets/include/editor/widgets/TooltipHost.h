//
// Created on 2026/11/15.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UITooltip.h>

#include <string>
#include <vector>

namespace sky::editor {

    class UiSkin;

    // One per surface. Paint after normal content, with the surface transform and clips restored.
    class TooltipHost {
    public:
        void Pointer(const sky::ui::UIPointerEvent &event);
        void Tick(sky::ui::UIElement *root, float delta);
        void Paint(sky::ui::UIPaintContext &context, const sky::ui::UIRect &surfaceBounds, const UiSkin &skin);
        // Abandon hover without releasing buttons held across a leave or keyboard event.
        void Cancel();
        void Reset();
        bool IsVisible() const;
        void ValidateRoot(sky::ui::UIElement *root);

    private:
        void ClearTarget();

        sky::ui::UIElement::InputReference rootReference;
        sky::ui::UIElement::InputReference owner;
        sky::ui::UITooltipTarget           target;
        sky::ui::UIRect                    anchor;
        sky::ui::UIPoint                   pointer;
        std::string                        text;
        std::vector<uint32_t>              pressedButtons;
        float                              elapsed       = 0.0f;
        bool                               pointerActive = false;
        bool                               built         = false;
    };

} // namespace sky::editor
