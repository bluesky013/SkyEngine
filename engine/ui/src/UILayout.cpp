//
// Created on 2026/09/19.
//

#include <ui/UILayout.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::ui {

    namespace {

        float NonNegative(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }

        float Coordinate(double value)
        {
            if (std::isnan(value)) {
                return 0.0f;
            }
            const double limit = std::numeric_limits<float>::max();
            return static_cast<float>(std::clamp(value, -limit, limit));
        }

        float Extent(double value)
        {
            return std::max(0.0f, Coordinate(value));
        }

        float Maximum(float value)
        {
            return std::isnan(value) || value < 0.0f ? 0.0f : value;
        }

        float ConstrainAxis(float desired, float minimum, float maximum)
        {
            if (std::isnan(desired) || desired < 0.0f) {
                desired = 0.0f;
            } else if (!std::isfinite(desired)) {
                desired = std::isfinite(maximum) ? maximum : minimum;
            }
            return std::clamp(desired, minimum, maximum);
        }

        void ResolveAxis(const UISizeRule &size, float anchorMin, float anchorMax, float offsetMin, float offsetMax, float &minimum, float &maximum)
        {
            float        resolved     = 0.0f;
            const float  parentExtent = maximum;
            const double insets       = static_cast<double>(Coordinate(offsetMin)) + Coordinate(offsetMax);
            if (anchorMin < anchorMax && std::isfinite(parentExtent)) {
                resolved = Extent((static_cast<double>(anchorMax) - anchorMin) * parentExtent - insets);
            } else if (size.mode == UISizeMode::FIXED) {
                resolved = NonNegative(size.value);
            } else if (size.mode == UISizeMode::FILL && std::isfinite(parentExtent)) {
                resolved = Extent(static_cast<double>(parentExtent) - insets);
            } else if (size.mode == UISizeMode::PERCENT && std::isfinite(parentExtent)) {
                resolved = Extent(static_cast<double>(parentExtent) * NonNegative(size.value));
            } else {
                return;
            }
            minimum = maximum = resolved;
        }

        float ResolveExtent(const UISizeRule &size, float parentExtent, float offsetMin, float offsetMax, float measured)
        {
            switch (size.mode) {
            case UISizeMode::FIXED: return NonNegative(size.value);
            case UISizeMode::FILL: return Extent(static_cast<double>(parentExtent) - offsetMin - offsetMax);
            case UISizeMode::PERCENT: return Extent(static_cast<double>(parentExtent) * NonNegative(size.value));
            case UISizeMode::AUTO:
            default: return NonNegative(measured);
            }
        }

    } // namespace

    UIConstraints UIConstraints::Normalized() const
    {
        UIConstraints result;
        result.maxSize = {Maximum(maxSize.width), Maximum(maxSize.height)};
        result.minSize = {std::min(NonNegative(minSize.width), result.maxSize.width), std::min(NonNegative(minSize.height), result.maxSize.height)};
        return result;
    }

    UISize UIConstraints::Constrain(UISize desired) const
    {
        const UIConstraints limits = Normalized();
        return {ConstrainAxis(desired.width, limits.minSize.width, limits.maxSize.width),
                ConstrainAxis(desired.height, limits.minSize.height, limits.maxSize.height)};
    }

    UIConstraints UIConstraints::Deflate(float left, float top, float right, float bottom) const
    {
        UIConstraints result     = Normalized();
        const double  horizontal = static_cast<double>(NonNegative(left)) + NonNegative(right);
        const double  vertical   = static_cast<double>(NonNegative(top)) + NonNegative(bottom);
        result.minSize           = {Extent(static_cast<double>(result.minSize.width) - horizontal),
                                    Extent(static_cast<double>(result.minSize.height) - vertical)};
        if (std::isfinite(result.maxSize.width)) {
            result.maxSize.width = Extent(static_cast<double>(result.maxSize.width) - horizontal);
        }
        if (std::isfinite(result.maxSize.height)) {
            result.maxSize.height = Extent(static_cast<double>(result.maxSize.height) - vertical);
        }
        return result;
    }

    UIConstraints ResolveMeasureConstraints(const UILayoutParams &layout, const UIConstraints &available)
    {
        UIConstraints result = available.Normalized();
        ResolveAxis(layout.width, layout.anchorMinX, layout.anchorMaxX, layout.offsetLeft, layout.offsetRight, result.minSize.width,
                    result.maxSize.width);
        ResolveAxis(layout.height, layout.anchorMinY, layout.anchorMaxY, layout.offsetTop, layout.offsetBottom, result.minSize.height,
                    result.maxSize.height);
        return result;
    }

    UIRect UILayoutParams::ContentRect(const UIRect &bounds) const
    {
        UIRect      content;
        const float right  = std::max(bounds.left, bounds.right);
        const float bottom = std::max(bounds.top, bounds.bottom);
        content.left       = std::min(right, Coordinate(static_cast<double>(bounds.left) + NonNegative(paddingLeft)));
        content.top        = std::min(bottom, Coordinate(static_cast<double>(bounds.top) + NonNegative(paddingTop)));
        content.right      = std::max(content.left, Coordinate(static_cast<double>(right) - NonNegative(paddingRight)));
        content.bottom     = std::max(content.top, Coordinate(static_cast<double>(bottom) - NonNegative(paddingBottom)));
        return content;
    }

    UIRect ComputeElementBounds(const UILayoutParams &layout, const UIRect &parentRect, float measuredWidth, float measuredHeight)
    {
        return ComputeElementBounds(layout, parentRect, {parentRect.Width(), parentRect.Height()}, measuredWidth, measuredHeight);
    }

    UISize UILayoutParams::ContentSize(UISize outer) const
    {
        return UIConstraints{{}, outer}.Deflate(paddingLeft, paddingTop, paddingRight, paddingBottom).maxSize;
    }

    UIRect ComputeElementBounds(const UILayoutParams &layout, const UIRect &parentRect, UISize parentSize, float measuredWidth, float measuredHeight)
    {
        UIRect       result;
        const UISize normalized   = UIConstraints{}.Constrain(parentSize);
        const float  parentWidth  = normalized.width;
        const float  parentHeight = normalized.height;

        if (layout.anchorMinX < layout.anchorMaxX) {
            result.left  = parentRect.left + layout.anchorMinX * parentWidth + layout.offsetLeft;
            result.right = parentRect.left + layout.anchorMaxX * parentWidth - layout.offsetRight;
        } else {
            const float base = parentRect.left + layout.anchorMinX * parentWidth + layout.offsetLeft;
            result.left      = base;
            result.right     = base + ResolveExtent(layout.width, parentWidth, layout.offsetLeft, layout.offsetRight, measuredWidth);
        }

        if (layout.anchorMinY < layout.anchorMaxY) {
            result.top    = parentRect.top + layout.anchorMinY * parentHeight + layout.offsetTop;
            result.bottom = parentRect.top + layout.anchorMaxY * parentHeight - layout.offsetBottom;
        } else {
            const float base = parentRect.top + layout.anchorMinY * parentHeight + layout.offsetTop;
            result.top       = base;
            result.bottom    = base + ResolveExtent(layout.height, parentHeight, layout.offsetTop, layout.offsetBottom, measuredHeight);
        }

        result.left   = Coordinate(result.left);
        result.top    = Coordinate(result.top);
        result.right  = std::max(result.left, Coordinate(result.right));
        result.bottom = std::max(result.top, Coordinate(result.bottom));
        return result;
    }

} // namespace sky::ui
