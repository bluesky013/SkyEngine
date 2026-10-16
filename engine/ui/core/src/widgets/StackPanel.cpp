//
// StackPanel implementation (see StackPanel.h).
//

#include <ui/widgets/StackPanel.h>

#include <detail/LayoutMeasurement.h>
#include <ui/detail/ChildRouting.h>

#include <algorithm>
#include <iterator>

namespace sky::ui {

    UIElement *StackPanel::Add(std::unique_ptr<UIElement> child, const StackSlot &slot)
    {
        UIElement *raw = AddChild(std::move(child));
        if (raw != nullptr) {
            slots.emplace(raw, slot);
        }
        return raw;
    }

    StackSlot StackPanel::GetSlot(const UIElement &child) const
    {
        const auto found = slots.find(&child);
        return found == slots.end() ? StackSlot{} : found->second;
    }

    void StackPanel::OnChildrenChanged()
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

    void StackPanel::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    UISize StackPanel::MeasureChildren(const UIConstraints &available, std::vector<UIRect> &rects, std::vector<UISize> *allocations)
    {
        const auto         &children    = GetChildren();
        const bool          horizontal  = orientation == StackOrientation::Horizontal;
        const float         mainBudget  = horizontal ? available.maxSize.width : available.maxSize.height;
        const bool          boundedMain = std::isfinite(mainBudget);
        const float         spacing     = detail::NonNegative(gap);
        std::vector<UISize> sizes(children.size());
        rects.resize(children.size());
        float      used      = children.empty() ? 0.0f : static_cast<float>(children.size() - 1) * spacing;
        float      weightSum = 0.0f;
        const auto measure   = [&](UIElement &child, const StackSlot &slot, float fillAllocation) {
            const float   margin = detail::NonNegative(slot.margin);
            UIConstraints budget = detail::Loose(available.Deflate(margin, margin, margin, margin));
            if (slot.width == StackSize::Fixed) {
                detail::SetWidth(budget, slot.fixedW);
            } else if (slot.width == StackSize::Fill && std::isfinite(available.maxSize.width)) {
                detail::SetWidth(budget, horizontal ? fillAllocation : budget.maxSize.width);
            }
            if (slot.height == StackSize::Fixed) {
                detail::SetHeight(budget, slot.fixedH);
            } else if (slot.height == StackSize::Fill && std::isfinite(available.maxSize.height)) {
                detail::SetHeight(budget, horizontal ? budget.maxSize.height : fillAllocation);
            }
            return detail::MeasureAtWidth(child, budget);
        };
        for (size_t index = 0; index < children.size(); ++index) {
            const StackSlot slot = GetSlot(*children[index]);
            used += 2.0f * detail::NonNegative(slot.margin);
            const bool fill = boundedMain && (horizontal ? slot.width : slot.height) == StackSize::Fill;
            if (fill) {
                weightSum += detail::NonNegative(slot.weight);
            } else {
                sizes[index] = measure(*children[index], slot, 0.0f);
                used += horizontal ? sizes[index].width : sizes[index].height;
            }
        }
        const float remaining = boundedMain ? std::max(0.0f, mainBudget - used) : 0.0f;
        float       cursor    = 0.0f;
        float       cross     = 0.0f;
        for (size_t index = 0; index < children.size(); ++index) {
            const StackSlot slot   = GetSlot(*children[index]);
            const float     margin = detail::NonNegative(slot.margin);
            if (boundedMain && (horizontal ? slot.width : slot.height) == StackSize::Fill) {
                const float allocation = weightSum > 0.0f ? remaining * detail::NonNegative(slot.weight) / weightSum : 0.0f;
                sizes[index]           = measure(*children[index], slot, allocation);
            }
            const UISize size = sizes[index];
            rects[index]      = horizontal ? UIRect{cursor + margin, margin, cursor + margin + size.width, margin + size.height}
                                           : UIRect{margin, cursor + margin, margin + size.width, cursor + margin + size.height};
            cursor += (horizontal ? size.width : size.height) + 2.0f * margin + spacing;
            cross = std::max(cross, (horizontal ? size.height : size.width) + 2.0f * margin);
        }
        const float main = children.empty() ? 0.0f : cursor - spacing;
        if (allocations != nullptr) {
            *allocations = std::move(sizes);
        }
        return horizontal ? UISize{main, cross} : UISize{cross, main};
    }

    UISize StackPanel::OnMeasure(const UIConstraints &constraints)
    {
        auto        layout = detail::Padding(GetLayout());
        const float inset  = detail::NonNegative(padding);
        layout.paddingLeft += inset;
        layout.paddingRight += inset;
        layout.paddingTop += inset;
        layout.paddingBottom += inset;
        const float   paddingW = layout.paddingLeft + layout.paddingRight;
        const float   paddingH = layout.paddingTop + layout.paddingBottom;
        UIConstraints content  = detail::Loose(constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom));
        std::vector<UIRect> rects;
        const UISize        preliminary = MeasureChildren(content, rects);
        const float         width       = constraints.Constrain({preliminary.width + paddingW, 0.0f}).width;
        content.maxSize.width           = layout.ContentSize({width, 0.0f}).width;
        const UISize measured           = MeasureChildren(content, rects);
        return {width, measured.height + paddingH};
    }

    void StackPanel::ArrangeChildren(const UIRect &parentContent)
    {
        const float    inset = detail::NonNegative(padding);
        UILayoutParams insets;
        insets.paddingLeft = insets.paddingTop = insets.paddingRight = insets.paddingBottom = inset;
        const UIRect content                                                                = insets.ContentRect(parentContent);
        auto         allocationInsets                                                       = detail::Padding(GetLayout());
        allocationInsets.paddingLeft += inset;
        allocationInsets.paddingRight += inset;
        allocationInsets.paddingTop += inset;
        allocationInsets.paddingBottom += inset;
        const UISize        size = allocationInsets.ContentSize(GetArrangedSize());
        std::vector<UIRect> rects;
        std::vector<UISize> allocations;
        MeasureChildren(detail::Available(size.width, size.height), rects, &allocations);
        const auto &children = GetChildren();
        for (size_t index = 0; index < children.size(); ++index) {
            detail::ArrangeAt(*children[index], rects[index], content, allocations[index]);
        }
    }

    void StackPanel::CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out)
    {
        if (captured != nullptr && (directChild == nullptr || captured == directChild)) {
            out.push_back({captured->GetInputReference(), capturedPointerId});
            captured          = nullptr;
            capturedPointerId = 0;
        }
    }

    void StackPanel::OnPointerCancel(uint32_t pointerId)
    {
        if (captured == nullptr || capturedPointerId != pointerId) {
            return;
        }
        capturedPointerId = 0;
        CancelChildPointer(captured, pointerId);
    }

    UIEventResult StackPanel::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/false, &capturedPointerId);
    }

} // namespace sky::ui
