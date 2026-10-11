//
// StackPanel implementation (see StackPanel.h).
//

#include <ui/widgets/StackPanel.h>

#include <ui/detail/ChildRouting.h>

#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    UIElement *StackPanel::Add(std::unique_ptr<UIElement> child, const StackSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        items.push_back(Item{raw, slot});
        return raw;
    }

    void StackPanel::Measure(float &outWidth, float &outHeight)
    {
        const bool horizontal = orientation == StackOrientation::Horizontal;
        float      mainSum    = 0.0f;
        float      crossMax   = 0.0f;
        for (auto &it : items) {
            float pw = 0.0f;
            float ph = 0.0f;
            if (it.element != nullptr) {
                it.element->Measure(pw, ph);
            }
            const StackSlot &s  = it.slot;
            const float      w  = (s.width == StackSize::Fixed) ? s.fixedW : (s.width == StackSize::Fill ? 0.0f : pw);
            const float      h  = (s.height == StackSize::Fixed) ? s.fixedH : (s.height == StackSize::Fill ? 0.0f : ph);
            const float      cw = w + s.margin * 2.0f;
            const float      ch = h + s.margin * 2.0f;
            if (horizontal) {
                mainSum += cw;
                crossMax = std::max(crossMax, ch);
            } else {
                mainSum += ch;
                crossMax = std::max(crossMax, cw);
            }
        }
        const float n     = static_cast<float>(items.size());
        const float gaps  = n > 1.0f ? (n - 1.0f) * gap : 0.0f;
        const float main  = mainSum + gaps + padding * 2.0f;
        const float cross = crossMax + padding * 2.0f;
        if (horizontal) {
            outWidth  = main;
            outHeight = cross;
        } else {
            outWidth  = cross;
            outHeight = main;
        }
    }

    void StackPanel::Arrange()
    {
        if (items.empty()) {
            return;
        }
        const UIRect bounds     = GetBounds();
        const bool   horizontal = orientation == StackOrientation::Horizontal;
        const UIRect content{bounds.left + padding, bounds.top + padding, bounds.right - padding, bounds.bottom - padding};
        const float  contentMain  = horizontal ? (content.right - content.left) : (content.bottom - content.top);
        const float  contentCross = horizontal ? (content.bottom - content.top) : (content.right - content.left);

        float used      = 0.0f;
        float weightSum = 0.0f;
        for (auto &it : items) {
            const StackSlot &s        = it.slot;
            const StackSize  mainMode = horizontal ? s.width : s.height;
            float            pw       = 0.0f;
            float            ph       = 0.0f;
            if (it.element != nullptr) {
                it.element->Measure(pw, ph);
            }
            if (mainMode == StackSize::Fixed) {
                used += horizontal ? s.fixedW : s.fixedH;
            } else if (mainMode == StackSize::Auto) {
                used += horizontal ? pw : ph;
            } else {
                weightSum += std::max(0.0f, s.weight);
            }
            used += s.margin * 2.0f;
        }
        const float gaps      = items.size() > 1 ? (static_cast<float>(items.size()) - 1.0f) * gap : 0.0f;
        const float remaining = std::max(0.0f, contentMain - used - gaps);

        float cursor = horizontal ? content.left : content.top;
        for (auto &it : items) {
            const StackSlot &s         = it.slot;
            const StackSize  mainMode  = horizontal ? s.width : s.height;
            const StackSize  crossMode = horizontal ? s.height : s.width;
            float            pw        = 0.0f;
            float            ph        = 0.0f;
            if (it.element != nullptr) {
                it.element->Measure(pw, ph);
            }

            float mainSize = 0.0f;
            if (mainMode == StackSize::Fixed) {
                mainSize = horizontal ? s.fixedW : s.fixedH;
            } else if (mainMode == StackSize::Auto) {
                mainSize = horizontal ? pw : ph;
            } else {
                mainSize = (weightSum > 0.0f) ? remaining * (std::max(0.0f, s.weight) / weightSum) : 0.0f;
            }

            float crossSize = 0.0f;
            if (crossMode == StackSize::Fixed) {
                crossSize = horizontal ? s.fixedH : s.fixedW;
            } else if (crossMode == StackSize::Auto) {
                crossSize = horizontal ? ph : pw;
            } else {
                crossSize = contentCross;
            }

            UIRect r;
            if (horizontal) {
                r.left   = cursor + s.margin;
                r.right  = cursor + mainSize - s.margin;
                r.top    = content.top + s.margin;
                r.bottom = r.top + crossSize;
            } else {
                r.top    = cursor + s.margin;
                r.bottom = cursor + mainSize - s.margin;
                r.left   = content.left + s.margin;
                r.right  = r.left + crossSize;
            }
            if (it.element != nullptr) {
                it.element->SetBounds(r);
            }
            cursor += mainSize + gap;
        }
    }

    UIEventResult StackPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false);
    }

    void StackPanel::OnPaint(UIPaintContext &context)
    {
        (void)context;
        Arrange();
    }

} // namespace sky::ui
