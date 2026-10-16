//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

namespace sky::ui {

    // Arranges children top to bottom with fixed spacing.
    class VBox : public UIElement {
    public:
        VBox()
        {
            SetLayoutVolatile(false);
        }
        ~VBox() override = default;

        const char *GetTypeName() const override
        {
            return "VBox";
        }

        void  SetSpacing(float value);
        float GetSpacing() const
        {
            return spacing;
        }

        void Measure(float &outWidth, float &outHeight) final;

    protected:
        UISize OnMeasure(const UIConstraints &constraints) override;
        void   ArrangeChildren(const UIRect &content) override;

    private:
        float spacing = 0.0f;
    };

} // namespace sky::ui
