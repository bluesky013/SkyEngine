//
// Overlay implementation (see Overlay.h).
//

#include <ui/widgets/Overlay.h>

#include <detail/LayoutMeasurement.h>
#include <ui/detail/ChildRouting.h>

#include <algorithm>

namespace sky::ui {

    void Overlay::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    UISize Overlay::OnMeasure(const UIConstraints &constraints)
    {
        const auto    layout   = detail::Padding(GetLayout());
        const float   paddingW = layout.paddingLeft + layout.paddingRight;
        UIConstraints content  = detail::Loose(constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom));
        float         naturalWidth = 0.0f;
        for (const auto &child : GetChildren()) {
            naturalWidth = std::max(naturalWidth, detail::MeasureAtWidth(*child, content).width);
        }
        const float width = constraints.Constrain({naturalWidth + paddingW, 0.0f}).width;
        detail::SetWidth(content, layout.ContentSize({width, 0.0f}).width);
        float height = 0.0f;
        for (const auto &child : GetChildren()) {
            height = std::max(height, child->MeasureDesired(content).height);
        }
        return {width, height + layout.paddingTop + layout.paddingBottom};
    }

    void Overlay::ArrangeChildren(const UIRect &content)
    {
        for (const auto &child : GetChildren()) {
            if (child) {
                child->Arrange(content, GetContentSize());
            }
        }
    }

    void Overlay::OnChildrenChanged()
    {
        const auto &children = GetChildren();
        if (std::none_of(children.begin(), children.end(), [this](const UIElementPtr &child) { return child.get() == captured; })) {
            captured = nullptr;
        }
    }

    void Overlay::CollectDetachedInput(UIElement *directChild, std::vector<InputCancellation> &out)
    {
        if (captured != nullptr && (directChild == nullptr || captured == directChild)) {
            out.push_back({captured->GetInputReference(), capturedPointerId});
            captured          = nullptr;
            capturedPointerId = 0;
        }
    }

    void Overlay::OnPointerCancel(uint32_t pointerId)
    {
        if (captured == nullptr || capturedPointerId != pointerId) {
            return;
        }
        capturedPointerId = 0;
        CancelChildPointer(captured, pointerId);
    }

    UIEventResult Overlay::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/true, &capturedPointerId);
    }

} // namespace sky::ui
