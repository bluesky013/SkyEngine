//
// GridPanel: a uniform rows x columns grid; each child is placed at a cell (with optional span).
// See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <vector>

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
        GridPanel()           = default;
        ~GridPanel() override = default;

        const char *GetTypeName() const override
        {
            return "GridPanel";
        }

        void SetGrid(int rowCount, int colCount)
        {
            rows = rowCount > 0 ? rowCount : 1;
            cols = colCount > 0 ? colCount : 1;
        }

        UIElement *Add(std::unique_ptr<UIElement> child, const GridSlot &slot = {});

        void          Measure(float &outWidth, float &outHeight) override;
        void          OnPaint(UIPaintContext &context) override;
        UIEventResult RoutePointer(const UIPointerEvent &event);
        void          Layout()
        {
            Arrange();
        }

    private:
        struct Item {
            UIElement *element = nullptr;
            GridSlot   slot;
        };

        void Arrange();

        int               rows = 1;
        int               cols = 1;
        std::vector<Item> items;
        UIElement        *captured = nullptr;
    };

} // namespace sky::ui
