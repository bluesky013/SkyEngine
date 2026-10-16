//
// Created on 2026/09/19.
//

#include <ui/widgets/ScrollView.h>

#include <algorithm>
#include <cmath>

namespace sky::ui {

    namespace {
        bool ScrollsX(ScrollAxes axes)
        {
            return (static_cast<uint8_t>(axes) & 1u) != 0;
        }
        bool ScrollsY(ScrollAxes axes)
        {
            return (static_cast<uint8_t>(axes) & 2u) != 0;
        }
        float FiniteScroll(float value)
        {
            return std::isfinite(value) ? value : 0.0f;
        }
        float Padding(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }
    } // namespace

    ScrollView::ScrollView()
    {
        SetLayoutVolatile(false);
        SetClipsChildren(true);
    }

    UIElement *ScrollView::SetContent(UIElementPtr content)
    {
        ClearChildren();
        contentElement = AddChild(std::move(content));
        MarkLayoutDirty();
        return contentElement;
    }

    void ScrollView::SetScroll(float x, float y)
    {
        const float nextX = FiniteScroll(x);
        const float nextY = FiniteScroll(y);
        if (scrollX == nextX && scrollY == nextY) {
            return;
        }
        scrollX = nextX;
        scrollY = nextY;
        MarkArrangeDirty();
    }

    void ScrollView::ScrollBy(float dx, float dy)
    {
        SetScroll(scrollX + dx, scrollY + dy);
    }

    void ScrollView::SetScrollAxes(ScrollAxes value)
    {
        if (axes == value) {
            return;
        }
        axes = value;
        MarkLayoutDirty();
    }

    UIEventResult ScrollView::OnPointerEvent(const UIPointerEvent &event)
    {
        if (event.action == UIPointerAction::WHEEL) {
            if (ScrollsY(axes)) {
                ScrollBy(0.0f, event.wheelDelta);
            } else if (ScrollsX(axes)) {
                ScrollBy(event.wheelDelta, 0.0f);
            } else {
                return UIEventResult::UNHANDLED;
            }
            return UIEventResult::HANDLED;
        }
        return UIElement::OnPointerEvent(event);
    }

    UISize ScrollView::MeasureContent(UISize viewport)
    {
        if (contentElement == nullptr) {
            return {};
        }
        const UIConstraints   reference{{}, viewport};
        UIConstraints         budget = ResolveMeasureConstraints(contentElement->GetLayout(), reference);
        const UILayoutParams &layout = contentElement->GetLayout();
        const auto            axis = [](float viewportExtent, bool scrolls, const UISizeRule &rule, float anchorMin, float anchorMax, float &minimum,
                             float &maximum) {
            if (scrolls) {
                // AUTO axes can grow past the viewport; explicitly resolved
                // FIXED/FILL/PERCENT axes keep their one viewport-based size.
                const bool known =
                    rule.mode == UISizeMode::FIXED ||
                    (std::isfinite(viewportExtent) && (anchorMin < anchorMax || rule.mode == UISizeMode::FILL || rule.mode == UISizeMode::PERCENT));
                if (!known) {
                    minimum = 0.0f;
                    maximum = std::numeric_limits<float>::infinity();
                }
            } else {
                maximum = std::min(maximum, viewportExtent);
                minimum = std::min(minimum, maximum);
            }
        };
        axis(viewport.width, ScrollsX(axes), layout.width, layout.anchorMinX, layout.anchorMaxX, budget.minSize.width, budget.maxSize.width);
        axis(viewport.height, ScrollsY(axes), layout.height, layout.anchorMinY, layout.anchorMaxY, budget.minSize.height, budget.maxSize.height);
        return contentElement->MeasureDesired(budget);
    }

    UISize ScrollView::OnMeasure(const UIConstraints &constraints)
    {
        const UILayoutParams &layout  = GetLayout();
        const UIConstraints   content = constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom);
        const float           horizontalPadding = Padding(layout.paddingLeft) + Padding(layout.paddingRight);
        const float           verticalPadding   = Padding(layout.paddingTop) + Padding(layout.paddingBottom);
        const UISize          first             = MeasureContent(content.maxSize);
        const UISize          viewport          = constraints.Constrain({first.width + horizontalPadding, first.height + verticalPadding});
        // Freeze the first allocated cross-axis before measuring dependent content.
        // In particular, a Percent child must not repeatedly shrink an AUTO viewport.
        const UIConstraints allocated =
            UIConstraints{viewport, viewport}.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom);
        if (axes == ScrollAxes::Horizontal) {
            const UISize finalContent = MeasureContent({content.maxSize.width, allocated.maxSize.height});
            return {finalContent.width + horizontalPadding, viewport.height};
        }
        const UISize finalContent = MeasureContent({allocated.maxSize.width, content.maxSize.height});
        return {viewport.width, finalContent.height + verticalPadding};
    }

    void ScrollView::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    void ScrollView::OnChildrenChanged()
    {
        UIElement *next = GetChildren().empty() ? nullptr : GetChildren().front().get();
        if (next != contentElement) {
            contentElement = next;
            contentWidth = contentHeight = 0.0f;
            scrollX = scrollY = 0.0f;
        }
    }

    bool ScrollView::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "scroll.axes" && value.type == UIPropertyValue::Type::STRING) {
            if (value.stringValue == "both") {
                SetScrollAxes(ScrollAxes::Both);
                return true;
            }
            if (value.stringValue == "horizontal") {
                SetScrollAxes(ScrollAxes::Horizontal);
                return true;
            }
            if (value.stringValue == "vertical") {
                SetScrollAxes(ScrollAxes::Vertical);
                return true;
            }
            if (value.stringValue == "none") {
                SetScrollAxes(ScrollAxes::None);
                return true;
            }
            return false;
        }
        return UIElement::SetProperty(path, value);
    }

    void ScrollView::ArrangeChildren(const UIRect &view)
    {
        if (contentElement == nullptr) {
            contentWidth = contentHeight = 0.0f;
            scrollX = scrollY = 0.0f;
            return;
        }

        const UISize viewport = GetContentSize();
        const UISize measured = MeasureContent(viewport);
        contentWidth          = measured.width;
        contentHeight         = measured.height;

        const float maxX = std::max(0.0f, contentWidth - viewport.width);
        const float maxY = std::max(0.0f, contentHeight - viewport.height);
        scrollX          = ScrollsX(axes) ? std::clamp(scrollX, 0.0f, maxX) : 0.0f;
        scrollY          = ScrollsY(axes) ? std::clamp(scrollY, 0.0f, maxY) : 0.0f;

        UIRect slot;
        slot.left   = view.left - scrollX;
        slot.top    = view.top - scrollY;
        slot.right  = slot.left + contentWidth;
        slot.bottom = slot.top + contentHeight;
        contentElement->Arrange(slot, measured);
    }

} // namespace sky::ui
