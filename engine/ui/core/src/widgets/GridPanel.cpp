//
// GridPanel implementation (see GridPanel.h).
//

#include <ui/widgets/GridPanel.h>

#include <ui/detail/ChildRouting.h>

#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    UIElement *GridPanel::Add(std::unique_ptr<UIElement> child, const GridSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        items.push_back(Item{raw, slot});
        return raw;
    }

    void GridPanel::Measure(float &outWidth, float &outHeight)
    {
        float maxW = 0.0f;
        float maxH = 0.0f;
        for (auto &it : items) {
            float pw = 0.0f;
            float ph = 0.0f;
            if (it.element != nullptr) {
                it.element->Measure(pw, ph);
            }
            maxW = std::max(maxW, pw);
            maxH = std::max(maxH, ph);
        }
        outWidth  = static_cast<float>(cols) * maxW;
        outHeight = static_cast<float>(rows) * maxH;
    }

    void GridPanel::Arrange()
    {
        const UIRect bounds = GetBounds();
        const float  cw     = (bounds.right - bounds.left) / static_cast<float>(cols);
        const float  ch     = (bounds.bottom - bounds.top) / static_cast<float>(rows);
        for (auto &it : items) {
            if (it.element == nullptr) {
                continue;
            }
            const GridSlot &s = it.slot;
            UIRect          r{bounds.left + static_cast<float>(s.col) * cw, bounds.top + static_cast<float>(s.row) * ch,
                     bounds.left + static_cast<float>(s.col + s.colSpan) * cw, bounds.top + static_cast<float>(s.row + s.rowSpan) * ch};
            r.left += s.margin;
            r.top += s.margin;
            r.right -= s.margin;
            r.bottom -= s.margin;
            if (r.right < r.left) {
                r.right = r.left;
            }
            if (r.bottom < r.top) {
                r.bottom = r.top;
            }
            it.element->SetBounds(r);
        }
    }

    void GridPanel::OnPaint(UIPaintContext &context)
    {
        (void)context;
        Arrange();
    }

    UIEventResult GridPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false);
    }

} // namespace sky::ui
