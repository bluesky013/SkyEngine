//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

namespace sky::ui {

    // Clipping container with a single content child and a clamped scroll offset.
    class ScrollView : public UIElement {
    public:
        ScrollView();
        ~ScrollView() override = default;

        const char *GetTypeName() const override { return "ScrollView"; }

        UIElement *SetContent(UIElementPtr content);
        UIElement *GetContent() const { return contentElement; }

        void SetScroll(float x, float y);
        void ScrollBy(float dx, float dy);
        float GetScrollX() const { return scrollX; }
        float GetScrollY() const { return scrollY; }
        float GetContentWidth() const { return contentWidth; }
        float GetContentHeight() const { return contentHeight; }

        UIEventResult OnPointerEvent(const UIPointerEvent &event) override;

    protected:
        void ArrangeChildren(const UIRect &view) override;

    private:
        UIElement *contentElement = nullptr;
        float scrollX = 0.0f;
        float scrollY = 0.0f;
        float contentWidth = 0.0f;
        float contentHeight = 0.0f;
    };

} // namespace sky::ui
