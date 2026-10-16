//
// Constraint-aware linear layout; child UILayoutParams retain ownership of sizing rules.
//

#include <detail/LayoutMeasurement.h>
#include <ui/widgets/VBox.h>

namespace sky::ui {

    void VBox::SetSpacing(float value)
    {
        if (spacing == value) {
            return;
        }
        spacing = value;
        MarkLayoutDirty();
    }

    void VBox::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    UISize VBox::OnMeasure(const UIConstraints &constraints)
    {
        const UILayoutParams layout = detail::Padding(GetLayout());
        UIConstraints content = detail::Loose(constraints.Deflate(layout.paddingLeft, layout.paddingTop, layout.paddingRight, layout.paddingBottom));
        const auto    preliminary  = detail::MeasureLinear(GetChildren(), content, spacing, false);
        const float   paddingWidth = layout.paddingLeft + layout.paddingRight;
        const float   width        = constraints.Constrain({preliminary.desired.width + paddingWidth, 0.0f}).width;
        content.maxSize.width      = layout.ContentSize({width, 0.0f}).width;
        const auto measured        = detail::MeasureLinear(GetChildren(), content, spacing, false);
        return {width, measured.desired.height + layout.paddingTop + layout.paddingBottom};
    }

    void VBox::ArrangeChildren(const UIRect &content)
    {
        const UISize size     = GetContentSize();
        const auto   measured = detail::MeasureLinear(GetChildren(), detail::Available(size.width, size.height), spacing, false);
        const auto  &children = GetChildren();
        for (size_t index = 0; index < children.size(); ++index) {
            detail::ArrangeAt(*children[index], measured.bounds[index], content, measured.sizes[index]);
        }
    }

} // namespace sky::ui
