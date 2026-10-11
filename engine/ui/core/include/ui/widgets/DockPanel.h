//
// DockPanel: a layout container that docks children to edges (Top/Bottom/Left/Right) in order,
// with the remaining space going to the Fill child. See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <vector>

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
        DockPanel()           = default;
        ~DockPanel() override = default;

        const char *GetTypeName() const override
        {
            return "DockPanel";
        }

        UIElement *Add(std::unique_ptr<UIElement> child, const DockSlot &slot = {});

        void          Measure(float &outWidth, float &outHeight) override;
        void          OnPaint(UIPaintContext &context) override;
        UIEventResult RoutePointer(const UIPointerEvent &event);
        // Force layout now (so a parent can read child bounds before painting them).
        void Layout()
        {
            Arrange();
        }

    private:
        struct Item {
            UIElement *element = nullptr;
            DockSlot   slot;
        };

        void Arrange();

        std::vector<Item> items;
        UIElement        *captured = nullptr;
    };

} // namespace sky::ui
