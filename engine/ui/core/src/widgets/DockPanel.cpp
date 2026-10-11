//
// DockPanel implementation (see DockPanel.h).
//

#include <ui/widgets/DockPanel.h>

#include <ui/detail/ChildRouting.h>

#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    UIElement *DockPanel::Add(std::unique_ptr<UIElement> child, const DockSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        items.push_back(Item{raw, slot});
        return raw;
    }

    void DockPanel::Measure(float &outWidth, float &outHeight)
    {
        float width  = 0.0f;
        float height = 0.0f;
        for (auto &it : items) {
            float pw = 0.0f;
            float ph = 0.0f;
            if (it.element != nullptr) {
                it.element->Measure(pw, ph);
            }
            const float m = it.slot.margin * 2.0f;
            switch (it.slot.side) {
            case DockSide::Top:
            case DockSide::Bottom:
                height += it.slot.size + m;
                width = std::max(width, pw + m);
                break;
            case DockSide::Left:
            case DockSide::Right:
                width += it.slot.size + m;
                height = std::max(height, ph + m);
                break;
            case DockSide::Fill:
            default:
                width += pw + m;
                height += ph + m;
                break;
            }
        }
        outWidth  = width;
        outHeight = height;
    }

    void DockPanel::Arrange()
    {
        if (items.empty()) {
            return;
        }
        UIRect remaining = GetBounds();
        for (auto &it : items) {
            if (it.element == nullptr) {
                continue;
            }
            const DockSlot &slot = it.slot;
            UIRect          r    = remaining;
            switch (slot.side) {
            case DockSide::Top:
                r.bottom      = remaining.top + slot.size;
                remaining.top = r.bottom;
                break;
            case DockSide::Bottom:
                r.top            = remaining.bottom - slot.size;
                remaining.bottom = r.top;
                break;
            case DockSide::Left:
                r.right        = remaining.left + slot.size;
                remaining.left = r.right;
                break;
            case DockSide::Right:
                r.left          = remaining.right - slot.size;
                remaining.right = r.left;
                break;
            case DockSide::Fill:
            default:
                r         = remaining;
                remaining = UIRect{remaining.left, remaining.top, remaining.left, remaining.top};
                break;
            }
            r.left += slot.margin;
            r.top += slot.margin;
            r.right -= slot.margin;
            r.bottom -= slot.margin;
            if (r.right < r.left) {
                r.right = r.left;
            }
            if (r.bottom < r.top) {
                r.bottom = r.top;
            }
            it.element->SetBounds(r);
        }
    }

    void DockPanel::OnPaint(UIPaintContext &context)
    {
        (void)context;
        Arrange();
    }

    UIEventResult DockPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false);
    }

} // namespace sky::ui
