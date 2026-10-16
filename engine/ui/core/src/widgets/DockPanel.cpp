//
// DockPanel implementation (see DockPanel.h).
//

#include <ui/widgets/DockPanel.h>

#include <detail/LayoutMeasurement.h>
#include <ui/detail/ChildRouting.h>

#include <algorithm>
#include <iterator>

namespace sky::ui {

    UIElement *DockPanel::Add(std::unique_ptr<UIElement> child, const DockSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        if (raw != nullptr) {
            slots.emplace(raw, slot);
        }
        return raw;
    }

    bool DockPanel::SetSlot(UIElement *child, const DockSlot &slot)
    {
        if (child == nullptr || child->GetParent() != this) {
            return false;
        }
        const DockSlot current = GetSlot(*child);
        if (current.side == slot.side && current.size == slot.size && current.margin == slot.margin) {
            return true;
        }
        slots[child] = slot;
        MarkLayoutDirty();
        return true;
    }

    DockSlot DockPanel::GetSlot(const UIElement &child) const
    {
        const auto found = slots.find(&child);
        return found == slots.end() ? DockSlot{} : found->second;
    }

    void DockPanel::OnChildrenChanged()
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

    void DockPanel::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    UISize DockPanel::OnMeasure(const UIConstraints &constraints)
    {
        const auto    layout   = detail::Padding(GetLayout());
        const float   paddingW = layout.paddingLeft + layout.paddingRight;
        UIConstraints content  = detail::Loose(constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom));
        // Each edge consumes the remaining budget before the next child is
        // measured. Reverse folding reconstructs the required outer rectangle.
        const auto measure = [&](UIConstraints remaining, bool exactWidth) {
            struct Demand {
                DockSlot slot;
                UISize   size;
            };
            std::vector<Demand> demands;
            for (const auto &child : GetChildren()) {
                const DockSlot slot   = GetSlot(*child);
                const float    margin = detail::NonNegative(slot.margin);
                const float    extent = detail::NonNegative(slot.size);
                UIConstraints  budget = detail::Loose(remaining.Deflate(margin, margin, margin, margin));
                if (slot.side == DockSide::Left || slot.side == DockSide::Right) {
                    detail::SetWidth(budget, std::max(0.0f, extent - 2.0f * margin));
                } else if (exactWidth && std::isfinite(budget.maxSize.width)) {
                    detail::SetWidth(budget, budget.maxSize.width);
                }
                if (slot.side == DockSide::Top || slot.side == DockSide::Bottom) {
                    detail::SetHeight(budget, std::max(0.0f, extent - 2.0f * margin));
                }
                demands.push_back({slot, detail::MeasureAtWidth(*child, budget)});
                if (slot.side == DockSide::Fill) {
                    break; // Fill owns all remaining space, as in Arrange.
                }
                if (slot.side == DockSide::Left || slot.side == DockSide::Right) {
                    remaining.maxSize.width = std::max(0.0f, remaining.maxSize.width - extent);
                } else {
                    remaining.maxSize.height = std::max(0.0f, remaining.maxSize.height - extent);
                }
            }
            UISize desired;
            for (auto it = demands.rbegin(); it != demands.rend(); ++it) {
                const float margin = 2.0f * detail::NonNegative(it->slot.margin);
                const float extent = detail::NonNegative(it->slot.size);
                switch (it->slot.side) {
                case DockSide::Top:
                case DockSide::Bottom:
                    desired.width = std::max(desired.width, it->size.width + margin);
                    desired.height += extent;
                    break;
                case DockSide::Left:
                case DockSide::Right:
                    desired.width += extent;
                    desired.height = std::max(desired.height, it->size.height + margin);
                    break;
                case DockSide::Fill: desired = {it->size.width + margin, it->size.height + margin}; break;
                }
            }
            return desired;
        };
        const UISize preliminary = measure(content, false);
        const float  width       = constraints.Constrain({preliminary.width + paddingW, 0.0f}).width;
        content.maxSize.width    = layout.ContentSize({width, 0.0f}).width;
        const UISize measured    = measure(content, true);
        return {width, measured.height + layout.paddingTop + layout.paddingBottom};
    }

    void DockPanel::ArrangeChildren(const UIRect &content)
    {
        UIRect remaining     = content;
        UISize remainingSize = GetContentSize();
        for (const auto &child : GetChildren()) {
            const DockSlot slot       = GetSlot(*child);
            UIRect         r          = remaining;
            UISize         allocation = remainingSize;
            const float    extent     = detail::NonNegative(slot.size);
            switch (slot.side) {
            case DockSide::Top:
                allocation.height    = extent;
                remainingSize.height = std::max(0.0f, remainingSize.height - extent);
                r.bottom             = remaining.top + detail::NonNegative(slot.size);
                remaining.top        = r.bottom;
                break;
            case DockSide::Bottom:
                allocation.height    = extent;
                remainingSize.height = std::max(0.0f, remainingSize.height - extent);
                r.top                = remaining.bottom - detail::NonNegative(slot.size);
                remaining.bottom     = r.top;
                break;
            case DockSide::Left:
                allocation.width    = extent;
                remainingSize.width = std::max(0.0f, remainingSize.width - extent);
                r.right             = remaining.left + detail::NonNegative(slot.size);
                remaining.left      = r.right;
                break;
            case DockSide::Right:
                allocation.width    = extent;
                remainingSize.width = std::max(0.0f, remainingSize.width - extent);
                r.left              = remaining.right - detail::NonNegative(slot.size);
                remaining.right     = r.left;
                break;
            case DockSide::Fill:
            default:
                remainingSize = {};
                r             = remaining;
                remaining     = UIRect{remaining.left, remaining.top, remaining.left, remaining.top};
                break;
            }
            remaining.right    = std::max(remaining.left, remaining.right);
            remaining.bottom   = std::max(remaining.top, remaining.bottom);
            const float margin = detail::NonNegative(slot.margin);
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
            child->Arrange(r, UIConstraints{{}, allocation}.Deflate(margin, margin, margin, margin).maxSize);
        }
    }

    void DockPanel::CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out)
    {
        if (captured != nullptr && (directChild == nullptr || captured == directChild)) {
            out.push_back({captured->GetInputReference(), capturedPointerId});
            captured          = nullptr;
            capturedPointerId = 0;
        }
    }

    void DockPanel::OnPointerCancel(uint32_t pointerId)
    {
        if (captured == nullptr || capturedPointerId != pointerId) {
            return;
        }
        capturedPointerId = 0;
        CancelChildPointer(captured, pointerId);
    }

    UIEventResult DockPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false, &capturedPointerId);
    }

} // namespace sky::ui
