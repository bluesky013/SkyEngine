//
// Created on 2026/09/19.
//

#include <ui/UILayout.h>

namespace sky::ui {

    namespace {

        float ResolveExtent(const UISizeRule &size, float parentExtent, float offsetMin, float offsetMax, float measured)
        {
            switch (size.mode) {
            case UISizeMode::FIXED:
                return size.value;
            case UISizeMode::FILL:
                return parentExtent - offsetMin - offsetMax;
            case UISizeMode::PERCENT:
                return parentExtent * size.value;
            case UISizeMode::AUTO:
            default:
                return measured;
            }
        }

    } // namespace

    UIRect UILayoutParams::ContentRect(const UIRect &bounds) const
    {
        UIRect content;
        content.left   = bounds.left + paddingLeft;
        content.top    = bounds.top + paddingTop;
        content.right  = bounds.right - paddingRight;
        content.bottom = bounds.bottom - paddingBottom;
        return content;
    }

    UIRect ComputeElementBounds(const UILayoutParams &layout,
                                const UIRect &parentRect,
                                float measuredWidth,
                                float measuredHeight)
    {
        UIRect result;
        const float parentWidth  = parentRect.Width();
        const float parentHeight = parentRect.Height();

        if (layout.anchorMinX < layout.anchorMaxX) {
            result.left  = parentRect.left + layout.anchorMinX * parentWidth + layout.offsetLeft;
            result.right = parentRect.left + layout.anchorMaxX * parentWidth - layout.offsetRight;
        } else {
            const float base = parentRect.left + layout.anchorMinX * parentWidth + layout.offsetLeft;
            result.left  = base;
            result.right = base + ResolveExtent(layout.width, parentWidth, layout.offsetLeft, layout.offsetRight, measuredWidth);
        }

        if (layout.anchorMinY < layout.anchorMaxY) {
            result.top    = parentRect.top + layout.anchorMinY * parentHeight + layout.offsetTop;
            result.bottom = parentRect.top + layout.anchorMaxY * parentHeight - layout.offsetBottom;
        } else {
            const float base = parentRect.top + layout.anchorMinY * parentHeight + layout.offsetTop;
            result.top    = base;
            result.bottom = base + ResolveExtent(layout.height, parentHeight, layout.offsetTop, layout.offsetBottom, measuredHeight);
        }

        return result;
    }

} // namespace sky::ui
