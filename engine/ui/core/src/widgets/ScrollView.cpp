//
// Created on 2026/09/19.
//

#include <ui/widgets/ScrollView.h>

#include <algorithm>

namespace sky::ui {

    ScrollView::ScrollView()
    {
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
        scrollX = x;
        scrollY = y;
        MarkLayoutDirty();
    }

    void ScrollView::ScrollBy(float dx, float dy)
    {
        scrollX += dx;
        scrollY += dy;
        MarkLayoutDirty();
    }

    UIEventResult ScrollView::OnPointerEvent(const UIPointerEvent &event)
    {
        if (event.action == UIPointerAction::WHEEL) {
            ScrollBy(0.0f, event.wheelDelta);
            return UIEventResult::HANDLED;
        }
        return UIElement::OnPointerEvent(event);
    }

    void ScrollView::ArrangeChildren(const UIRect &view)
    {
        if (contentElement == nullptr) {
            return;
        }

        float measuredWidth = 0.0f;
        float measuredHeight = 0.0f;
        contentElement->Measure(measuredWidth, measuredHeight);
        const UILayoutParams &contentLayout = contentElement->GetLayout();
        if (contentLayout.width.mode == UISizeMode::FIXED) {
            measuredWidth = contentLayout.width.value;
        }
        if (contentLayout.height.mode == UISizeMode::FIXED) {
            measuredHeight = contentLayout.height.value;
        }
        contentWidth = measuredWidth;
        contentHeight = measuredHeight;

        const float maxX = std::max(0.0f, contentWidth - view.Width());
        const float maxY = std::max(0.0f, contentHeight - view.Height());
        scrollX = std::clamp(scrollX, 0.0f, maxX);
        scrollY = std::clamp(scrollY, 0.0f, maxY);

        UIRect slot;
        slot.left = view.left - scrollX;
        slot.top = view.top - scrollY;
        slot.right = slot.left + contentWidth;
        slot.bottom = slot.top + contentHeight;
        contentElement->Layout(slot);
    }

} // namespace sky::ui
