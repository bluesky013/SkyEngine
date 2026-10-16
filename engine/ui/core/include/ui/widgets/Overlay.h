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
        Overlay()
        {
            SetLayoutVolatile(false);
        }
        ~Overlay() override = default;

        const char *GetTypeName() const override
        {
            return "Overlay";
        }

        void          Measure(float &outWidth, float &outHeight) final;
        UIEventResult RoutePointer(const UIPointerEvent &event);
        void          OnPointerCancel(uint32_t pointerId) override;

    protected:
        UISize OnMeasure(const UIConstraints &constraints) override;
        void   ArrangeChildren(const UIRect &content) override;
        void   OnChildrenChanged() override;
        void   CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out) override;

    private:
        UIElement *captured          = nullptr;
        uint32_t   capturedPointerId = 0;
    };

} // namespace sky::ui
