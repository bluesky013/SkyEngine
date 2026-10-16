//
// StackPanel: the first layout container (see docs/editor/ui-layout-and-chrome-plan.md).
// Arranges its children in a single row/column from slot metadata (Fixed/Auto/Fill + margin),
// so owners declare intent instead of computing pixel rects. Arrange completes the
// entire child layout before painting or hit-testing.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <unordered_map>
#include <vector>

namespace sky::ui {

    enum class StackOrientation {
        Horizontal = 0,
        Vertical,
    };

    enum class StackSize {
        Auto = 0, // use the child's preferred (Measure) size
        Fixed,    // use the slot's fixed pixel size
        Fill,     // take remaining space, split by weight
    };

    struct StackSlot {
        StackSize width  = StackSize::Auto;
        StackSize height = StackSize::Auto;
        float     fixedW = 0.0f;
        float     fixedH = 0.0f;
        float     weight = 1.0f; // relative share for Fill
        float     margin = 0.0f; // uniform outer margin around the item
    };

    class StackPanel : public UIElement {
    public:
        StackPanel()
        {
            SetLayoutVolatile(false);
        }
        explicit StackPanel(StackOrientation orientation) : orientation(orientation)
        {
            SetLayoutVolatile(false);
        }
        ~StackPanel() override = default;

        const char *GetTypeName() const override
        {
            return "StackPanel";
        }

        void SetOrientation(StackOrientation value)
        {
            if (orientation == value) {
                return;
            }
            orientation = value;
            MarkLayoutDirty();
        }
        void SetGap(float value)
        {
            if (gap == value) {
                return;
            }
            gap = value;
            MarkLayoutDirty();
        }
        void SetPadding(float value)
        {
            if (padding == value) {
                return;
            }
            padding = value;
            MarkLayoutDirty();
        }

        // Adds a child with an explicit slot. Returns the child (owned by the element tree).
        UIElement *Add(std::unique_ptr<UIElement> child, const StackSlot &slot = {});

        void Measure(float &outWidth, float &outHeight) final;

        // Delivers a pointer event to the child under the cursor (or the pressed child), so a panel
        // can host interactive children (buttons). Returns UNHANDLED when no child is hit.
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
        StackSlot GetSlot(const UIElement &child) const;
        UISize    MeasureChildren(const UIConstraints &available, std::vector<UIRect> &rects, std::vector<UISize> *allocations = nullptr);

        StackOrientation orientation = StackOrientation::Horizontal;
        float            gap         = 0.0f;
        float            padding     = 0.0f;
        // Inherited AddChild uses a default slot; live child order owns layout.
        std::unordered_map<const UIElement *, StackSlot> slots;
        UIElement                                       *captured          = nullptr;
        uint32_t                                         capturedPointerId = 0;
    };

} // namespace sky::ui
