//
// DockPanel: a layout container that docks children to edges (Top/Bottom/Left/Right) in order,
// with the remaining space going to the Fill child. See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <unordered_map>

namespace sky::ui {

    enum class DockSide {
        Top = 0,
        Bottom,
        Left,
        Right,
        Fill,
    };

    struct DockSlot {
        DockSide side   = DockSide::Fill;
        float    size   = 0.0f; // extent along the docked axis (ignored for Fill)
        float    margin = 0.0f; // uniform inner margin
    };

    class DockPanel : public UIElement {
    public:
        DockPanel()
        {
            SetLayoutVolatile(false);
        }
        ~DockPanel() override = default;

        const char *GetTypeName() const override
        {
            return "DockPanel";
        }

        UIElement *Add(std::unique_ptr<UIElement> child, const DockSlot &slot = {});
        // Update a live child's allocation when host metrics change.
        bool SetSlot(UIElement *child, const DockSlot &slot);

        void          Measure(float &outWidth, float &outHeight) final;
        UIEventResult RoutePointer(const UIPointerEvent &event);
        void          OnPointerCancel(uint32_t pointerId) override;
        using UIElement::Layout;
        // Compatibility for hosts that already assigned this panel's bounds.
        void Layout()
        {
            Arrange(GetBounds(), GetArrangedSize());
        }

    protected:
        UISize OnMeasure(const UIConstraints &constraints) override;
        void   ArrangeChildren(const UIRect &content) override;
        void   OnChildrenChanged() override;
        void   CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out) override;

    private:
        DockSlot GetSlot(const UIElement &child) const;

        // Inherited AddChild uses a default slot; live child order owns layout.
        std::unordered_map<const UIElement *, DockSlot> slots;
        UIElement                                      *captured          = nullptr;
        uint32_t                                        capturedPointerId = 0;
    };

} // namespace sky::ui
