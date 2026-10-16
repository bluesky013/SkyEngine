//
// GridPanel: a uniform rows x columns grid; each child is placed at a cell (with optional span).
// See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <unordered_map>

namespace sky::ui {

    struct GridSlot {
        int   row     = 0;
        int   col     = 0;
        int   rowSpan = 1;
        int   colSpan = 1;
        float margin  = 0.0f;
    };

    class GridPanel : public UIElement {
    public:
        GridPanel()
        {
            SetLayoutVolatile(false);
        }
        ~GridPanel() override = default;

        const char *GetTypeName() const override
        {
            return "GridPanel";
        }

        void SetGrid(int rowCount, int colCount)
        {
            const int nextRows = rowCount > 0 ? rowCount : 1;
            const int nextCols = colCount > 0 ? colCount : 1;
            if (rows == nextRows && cols == nextCols) {
                return;
            }
            rows = nextRows;
            cols = nextCols;
            MarkLayoutDirty();
        }

        UIElement *Add(std::unique_ptr<UIElement> child, const GridSlot &slot = {});

        void          Measure(float &outWidth, float &outHeight) final;
        UIEventResult RoutePointer(const UIPointerEvent &event);
        void          OnPointerCancel(uint32_t pointerId) override;
        using UIElement::Layout;
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
        GridSlot GetSlot(const UIElement &child) const;

        int rows = 1;
        int cols = 1;
        // Inherited AddChild uses a default slot; live child order owns layout.
        std::unordered_map<const UIElement *, GridSlot> slots;
        UIElement                                      *captured          = nullptr;
        uint32_t                                        capturedPointerId = 0;
    };

} // namespace sky::ui
