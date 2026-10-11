//
// StackPanel: the first layout container (see docs/editor/ui-layout-and-chrome-plan.md).
// Arranges its children in a single row/column from slot metadata (Fixed/Auto/Fill + margin),
// so owners declare intent instead of computing pixel rects. Layout runs in OnPaint (the
// framework has a Measure pass but no separate Arrange hook); children paint afterwards.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

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
        float     margin = 0.0f; // uniform inner margin around the item
    };

    class StackPanel : public UIElement {
    public:
        StackPanel() = default;
        explicit StackPanel(StackOrientation orientation) : orientation(orientation)
        {
        }
        ~StackPanel() override = default;

        const char *GetTypeName() const override
        {
            return "StackPanel";
        }

        void SetOrientation(StackOrientation value)
        {
            orientation = value;
        }
        void SetGap(float value)
        {
            gap = value;
        }
        void SetPadding(float value)
        {
            padding = value;
        }

        // Adds a child with an explicit slot. Returns the child (owned by the element tree).
        UIElement *Add(std::unique_ptr<UIElement> child, const StackSlot &slot = {});

        void Measure(float &outWidth, float &outHeight) override;
        void OnPaint(UIPaintContext &context) override;

        // Delivers a pointer event to the child under the cursor (or the pressed child), so a panel
        // can host interactive children (buttons). Returns UNHANDLED when no child is hit.
        UIEventResult RoutePointer(const UIPointerEvent &event);

        // Force layout now (so a parent can read child bounds before painting them).
        void Layout()
        {
            Arrange();
        }

    private:
        struct Item {
            UIElement *element = nullptr;
            StackSlot  slot;
        };

        void Arrange();

        StackOrientation  orientation = StackOrientation::Horizontal;
        float             gap         = 0.0f;
        float             padding     = 0.0f;
        std::vector<Item> items;
        UIElement        *captured = nullptr;
    };

} // namespace sky::ui
