//
// Overlay: a layout container where every child fills the same rect (stacked; the last added is on
// top). Used for overlays/popups/scrim layers. See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

namespace sky::ui {

    class Overlay : public UIElement {
    public:
        Overlay()           = default;
        ~Overlay() override = default;

        const char *GetTypeName() const override
        {
            return "Overlay";
        }

        void          OnPaint(UIPaintContext &context) override;
        UIEventResult RoutePointer(const UIPointerEvent &event);

    private:
        UIElement *captured = nullptr;
    };

} // namespace sky::ui
