//
// Created on 2026/09/19.
//

#include <ui/widgets/HBox.h>

#include <algorithm>

namespace sky::ui {

    namespace {

        // Intrinsic child size: the base Measure has no content size, so honor a
        // FIXED rule here; PERCENT/FILL resolve against the parent during Layout.
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

    void HBox::SetSpacing(float value)
    {
        spacing = value;
        MarkLayoutDirty();
    }

    void HBox::Measure(float &outWidth, float &outHeight)
    {
        const UILayoutParams &params = GetLayout();
        float total = 0.0f;
        float maxHeight = 0.0f;
        int count = 0;

        for (const auto &child : GetChildren()) {
            float childWidth = 0.0f;
            float childHeight = 0.0f;
            ChildExtent(child.get(), childWidth, childHeight);
            total += childWidth;
            maxHeight = std::max(maxHeight, childHeight);
            count++;
        }
        if (count > 1) {
            total += spacing * static_cast<float>(count - 1);
        }

        outWidth = total + params.paddingLeft + params.paddingRight;
        outHeight = maxHeight + params.paddingTop + params.paddingBottom;
    }

    void HBox::ArrangeChildren(const UIRect &content)
    {
        float x = content.left;
        for (const auto &child : GetChildren()) {
            float childWidth = 0.0f;
            float childHeight = 0.0f;
            ChildExtent(child.get(), childWidth, childHeight);

            UIRect slot;
            slot.left = x;
            slot.top = content.top;
            slot.right = content.right;
            slot.bottom = content.bottom;
            child->Layout(slot);

            x += childWidth + spacing;
        }
    }

} // namespace sky::ui
