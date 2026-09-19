//
// Created on 2026/09/19.
//

#include <ui/widgets/VBox.h>

#include <algorithm>

namespace sky::ui {

    namespace {

        void ChildExtent(UIElement *child, float &width, float &height)
        {
            child->Measure(width, height);
            const UILayoutParams &params = child->GetLayout();
            if (params.width.mode == UISizeMode::FIXED) {
                width = params.width.value;
            }
            if (params.height.mode == UISizeMode::FIXED) {
                height = params.height.value;
            }
        }

    } // namespace

    void VBox::SetSpacing(float value)
    {
        spacing = value;
        MarkLayoutDirty();
    }

    void VBox::Measure(float &outWidth, float &outHeight)
    {
        const UILayoutParams &params = GetLayout();
        float total = 0.0f;
        float maxWidth = 0.0f;
        int count = 0;

        for (const auto &child : GetChildren()) {
            float childWidth = 0.0f;
            float childHeight = 0.0f;
            ChildExtent(child.get(), childWidth, childHeight);
            total += childHeight;
            maxWidth = std::max(maxWidth, childWidth);
            count++;
        }
        if (count > 1) {
            total += spacing * static_cast<float>(count - 1);
        }

        outWidth = maxWidth + params.paddingLeft + params.paddingRight;
        outHeight = total + params.paddingTop + params.paddingBottom;
    }

    void VBox::ArrangeChildren(const UIRect &content)
    {
        float y = content.top;
        for (const auto &child : GetChildren()) {
            float childWidth = 0.0f;
            float childHeight = 0.0f;
            ChildExtent(child.get(), childWidth, childHeight);

            UIRect slot;
            slot.left = content.left;
            slot.top = y;
            slot.right = content.right;
            slot.bottom = content.bottom;
            child->Layout(slot);

            y += childHeight + spacing;
        }
    }

} // namespace sky::ui
