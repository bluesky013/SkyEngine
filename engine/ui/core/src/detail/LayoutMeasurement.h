// Container measurement helpers. Runtime layout policy stays in the UI plugin.
#pragma once

#include <ui/UIElement.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sky::ui::detail {

    inline float NonNegative(float value)
    {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    inline UILayoutParams Padding(UILayoutParams layout)
    {
        layout.paddingLeft   = NonNegative(layout.paddingLeft);
        layout.paddingTop    = NonNegative(layout.paddingTop);
        layout.paddingRight  = NonNegative(layout.paddingRight);
        layout.paddingBottom = NonNegative(layout.paddingBottom);
        return layout;
    }

    inline UIConstraints Loose(UIConstraints constraints)
    {
        constraints.minSize = {};
        return constraints.Normalized();
    }

    inline UIConstraints Available(float width, float height)
    {
        UIConstraints constraints;
        constraints.maxSize = {width, height};
        return constraints.Normalized();
    }

    inline void SetWidth(UIConstraints &constraints, float width)
    {
        constraints.minSize.width = constraints.maxSize.width = NonNegative(width);
    }

    inline void SetHeight(UIConstraints &constraints, float height)
    {
        constraints.minSize.height = constraints.maxSize.height = NonNegative(height);
    }

    // Resolve width before accepting a width-dependent AUTO height. Explicit
    // slot constraints are already tight and need no intrinsic-width probe.
    inline UISize MeasureAtWidth(UIElement &element, UIConstraints constraints)
    {
        constraints    = constraints.Normalized();
        UISize desired = element.MeasureDesired(constraints);
        if (constraints.minSize.width != constraints.maxSize.width && desired.width != constraints.maxSize.width) {
            SetWidth(constraints, desired.width);
            desired = element.MeasureDesired(constraints);
        }
        return desired;
    }

    inline UIRect ResolveRect(const UILayoutParams &layout, const UIConstraints &available, const UISize &desired)
    {
        UILayoutParams finite        = layout;
        const bool     boundedWidth  = std::isfinite(available.maxSize.width);
        const bool     boundedHeight = std::isfinite(available.maxSize.height);
        if (!boundedWidth) {
            finite.anchorMinX = finite.anchorMaxX = 0.0f;
            finite.width                          = {UISizeMode::FIXED, desired.width};
        }
        if (!boundedHeight) {
            finite.anchorMinY = finite.anchorMaxY = 0.0f;
            finite.height                         = {UISizeMode::FIXED, desired.height};
        }
        return ComputeElementBounds(
            finite, {0.0f, 0.0f, boundedWidth ? available.maxSize.width : desired.width, boundedHeight ? available.maxSize.height : desired.height},
            desired.width, desired.height);
    }

    struct LinearLayout {
        UISize              desired;
        std::vector<UIRect> bounds;
        std::vector<UISize> sizes;
    };

    inline LinearLayout MeasureLinear(const std::vector<UIElementPtr> &children, UIConstraints available, float spacing, bool horizontal)
    {
        available               = Loose(available);
        spacing                 = NonNegative(spacing);
        const float mainBudget  = horizontal ? available.maxSize.width : available.maxSize.height;
        const bool  boundedMain = std::isfinite(mainBudget);
        const auto  fills       = [horizontal, boundedMain](const UIElement &child) {
            const UILayoutParams &layout = child.GetLayout();
            return boundedMain && (horizontal ? layout.width.mode == UISizeMode::FILL && !(layout.anchorMinX < layout.anchorMaxX)
                                                     : layout.height.mode == UISizeMode::FILL && !(layout.anchorMinY < layout.anchorMaxY));
        };
        LinearLayout result;
        result.bounds.resize(children.size());
        result.sizes.resize(children.size());
        float  used      = children.empty() ? 0.0f : static_cast<float>(children.size() - 1) * spacing;
        size_t fillCount = 0;
        for (size_t index = 0; index < children.size(); ++index) {
            UIElement &child = *children[index];
            if (fills(child)) {
                ++fillCount;
                continue;
            }
            const UIConstraints resolved = ResolveMeasureConstraints(child.GetLayout(), available);
            const UISize        desired  = MeasureAtWidth(child, resolved);
            result.bounds[index]         = ResolveRect(child.GetLayout(), available, desired);
            result.sizes[index]          = desired;
            used += std::max(0.0f, horizontal ? result.bounds[index].right : result.bounds[index].bottom);
        }
        const float share  = fillCount == 0 ? 0.0f : std::max(0.0f, mainBudget - used) / static_cast<float>(fillCount);
        float       cursor = 0.0f;
        float       cross  = 0.0f;
        for (size_t index = 0; index < children.size(); ++index) {
            UIElement &child = *children[index];
            const bool fill  = fills(child);
            if (fill) {
                UIConstraints allocation = available;
                if (horizontal) {
                    allocation.maxSize.width = share;
                } else {
                    allocation.maxSize.height = share;
                }
                const UISize desired = MeasureAtWidth(child, ResolveMeasureConstraints(child.GetLayout(), allocation));
                result.bounds[index] = ResolveRect(child.GetLayout(), allocation, desired);
                result.sizes[index]  = desired;
            }
            UIRect     &rect   = result.bounds[index];
            const float extent = std::max(0.0f, horizontal ? rect.right : rect.bottom);
            cross              = std::max(cross, std::max(0.0f, horizontal ? rect.bottom : rect.right));
            if (horizontal) {
                rect.left += cursor;
                rect.right += cursor;
            } else {
                rect.top += cursor;
                rect.bottom += cursor;
            }
            cursor += (fill ? std::max(share, extent) : extent) + spacing;
        }
        const float main = children.empty() ? 0.0f : cursor - spacing;
        result.desired   = horizontal ? UISize{main, cross} : UISize{cross, main};
        return result;
    }

    inline void ArrangeAt(UIElement &child, UIRect bounds, const UIRect &content, UISize logicalSize)
    {
        bounds.left += content.left;
        bounds.right += content.left;
        bounds.top += content.top;
        bounds.bottom += content.top;
        child.Arrange(bounds, logicalSize);
    }

} // namespace sky::ui::detail
