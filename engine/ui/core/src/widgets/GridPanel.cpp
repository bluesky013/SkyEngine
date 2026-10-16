//
// GridPanel implementation (see GridPanel.h).
//

#include <ui/widgets/GridPanel.h>

#include <detail/LayoutMeasurement.h>
#include <ui/detail/ChildRouting.h>

#include <algorithm>
#include <iterator>

namespace sky::ui {

    UIElement *GridPanel::Add(std::unique_ptr<UIElement> child, const GridSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        if (raw != nullptr) {
            slots.emplace(raw, slot);
        }
        return raw;
    }

    GridSlot GridPanel::GetSlot(const UIElement &child) const
    {
        const auto found = slots.find(&child);
        return found == slots.end() ? GridSlot{} : found->second;
    }

    void GridPanel::OnChildrenChanged()
    {
        const auto &children = GetChildren();
        for (auto it = slots.begin(); it != slots.end();) {
            const auto found =
                std::find_if(children.begin(), children.end(), [element = it->first](const UIElementPtr &child) { return child.get() == element; });
            it = found == children.end() ? slots.erase(it) : std::next(it);
        }
        if (std::none_of(children.begin(), children.end(), [this](const UIElementPtr &child) { return child.get() == captured; })) {
            captured = nullptr;
        }
    }

    void GridPanel::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    UISize GridPanel::OnMeasure(const UIConstraints &constraints)
    {
        const auto    layout   = detail::Padding(GetLayout());
        const float   paddingW = layout.paddingLeft + layout.paddingRight;
        UIConstraints content  = detail::Loose(constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom));
        const auto    measure  = [&](UIConstraints available, bool exactWidth) {
            float cellWidth  = 0.0f;
            float cellHeight = 0.0f;
            for (const auto &child : GetChildren()) {
                const GridSlot slot    = GetSlot(*child);
                const float    colSpan = static_cast<float>(std::max(1, slot.colSpan));
                const float    rowSpan = static_cast<float>(std::max(1, slot.rowSpan));
                const float    margin  = detail::NonNegative(slot.margin);
                UIConstraints  budget  = detail::Available(available.maxSize.width * (colSpan / static_cast<float>(cols)),
                                                               available.maxSize.height * (rowSpan / static_cast<float>(rows)))
                                           .Deflate(margin, margin, margin, margin);
                if (exactWidth && std::isfinite(budget.maxSize.width)) {
                    detail::SetWidth(budget, budget.maxSize.width);
                }
                const UISize desired = detail::MeasureAtWidth(*child, budget);
                cellWidth            = std::max(cellWidth, (desired.width + 2.0f * margin) / colSpan);
                cellHeight           = std::max(cellHeight, (desired.height + 2.0f * margin) / rowSpan);
            }
            return UISize{cellWidth * static_cast<float>(cols), cellHeight * static_cast<float>(rows)};
        };
        const UISize preliminary = measure(content, false);
        const float  width       = constraints.Constrain({preliminary.width + paddingW, 0.0f}).width;
        content.maxSize.width    = layout.ContentSize({width, 0.0f}).width;
        const UISize measured    = measure(content, true);
        return {width, measured.height + layout.paddingTop + layout.paddingBottom};
    }

    void GridPanel::ArrangeChildren(const UIRect &content)
    {
        const UIRect bounds = content;
        const UISize size   = GetContentSize();
        const float  cw     = size.width / static_cast<float>(cols);
        const float  ch     = size.height / static_cast<float>(rows);
        for (const auto &child : GetChildren()) {
            const GridSlot s = GetSlot(*child);
            UIRect         r{bounds.left + static_cast<float>(s.col) * cw, bounds.top + static_cast<float>(s.row) * ch,
                     bounds.left + static_cast<float>(s.col + std::max(1, s.colSpan)) * cw,
                     bounds.top + static_cast<float>(s.row + std::max(1, s.rowSpan)) * ch};
            const float    margin = detail::NonNegative(s.margin);
            r.left += margin;
            r.top += margin;
            r.right -= margin;
            r.bottom -= margin;
            if (r.right < r.left) {
                r.right = r.left;
            }
            if (r.bottom < r.top) {
                r.bottom = r.top;
            }
            const UISize allocation = UIConstraints{{},
                                                    {size.width * (static_cast<float>(std::max(1, s.colSpan)) / static_cast<float>(cols)),
                                                     size.height * (static_cast<float>(std::max(1, s.rowSpan)) / static_cast<float>(rows))}}
                                          .Deflate(margin, margin, margin, margin)
                                          .maxSize;
            child->Arrange(r, allocation);
        }
    }

    void GridPanel::CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out)
    {
        if (captured != nullptr && (directChild == nullptr || captured == directChild)) {
            out.push_back({captured->GetInputReference(), capturedPointerId});
            captured          = nullptr;
            capturedPointerId = 0;
        }
    }

    void GridPanel::OnPointerCancel(uint32_t pointerId)
    {
        if (captured == nullptr || capturedPointerId != pointerId) {
            return;
        }
        capturedPointerId = 0;
        CancelChildPointer(captured, pointerId);
    }

    UIEventResult GridPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false, &capturedPointerId);
    }

} // namespace sky::ui
