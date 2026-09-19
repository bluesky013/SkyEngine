//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

namespace sky::ui {

    // Arranges children top to bottom with fixed spacing.
    class VBox : public UIElement {
    public:
        VBox() = default;
        ~VBox() override = default;

        const char *GetTypeName() const override { return "VBox"; }

        void SetSpacing(float value);
        float GetSpacing() const { return spacing; }

        void Measure(float &outWidth, float &outHeight) override;

    protected:
        void ArrangeChildren(const UIRect &content) override;

    private:
        float spacing = 0.0f;
    };

} // namespace sky::ui
