//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

namespace sky::ui {

    class Panel : public UIElement {
    public:
        Panel();
        ~Panel() override = default;

        const char *GetTypeName() const override { return "Panel"; }

        void OnPaint(UIPaintContext &context) override;
    };

} // namespace sky::ui
