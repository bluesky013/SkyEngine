//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

namespace sky::ui {

    enum class ScrollAxes : uint8_t {
        None       = 0,
        Horizontal = 1,
        Vertical   = 2,
        Both       = 3,
    };

    // Clipping container with a single content child and a clamped scroll offset.
    class ScrollView : public UIElement {
    public:
        ScrollView();
        ~ScrollView() override = default;

        const char *GetTypeName() const override
        {
            return "ScrollView";
        }

        UIElement *SetContent(UIElementPtr content);
        UIElement *GetContent() const
        {
            return contentElement;
        }

        void  SetScroll(float x, float y);
        void  ScrollBy(float dx, float dy);
        float GetScrollX() const
        {
            return scrollX;
        }
        float GetScrollY() const
        {
            return scrollY;
        }
        float GetContentWidth() const
        {
            return contentWidth;
        }
        float GetContentHeight() const
        {
            return contentHeight;
        }

        void       SetScrollAxes(ScrollAxes value);
        ScrollAxes GetScrollAxes() const
        {
            return axes;
        }

        UIEventResult OnPointerEvent(const UIPointerEvent &event) override;
        void          Measure(float &outWidth, float &outHeight) final;
        bool          SetProperty(const std::string &path, const UIPropertyValue &value) override;

    protected:
        UISize OnMeasure(const UIConstraints &constraints) override;
        void   ArrangeChildren(const UIRect &view) override;
        void   OnChildrenChanged() override;

    private:
        UISize     MeasureContent(UISize viewport);
        UIElement *contentElement = nullptr;
        ScrollAxes axes           = ScrollAxes::Both;
        float      scrollX        = 0.0f;
        float      scrollY        = 0.0f;
        float      contentWidth   = 0.0f;
        float      contentHeight  = 0.0f;
    };

} // namespace sky::ui
