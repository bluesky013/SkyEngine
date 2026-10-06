//
// Created on 2026/10/06.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIRect.h>

namespace sky::ui {
    class UIPaintContext;
} // namespace sky::ui

namespace sky::editor {

    // Base for engine-drawn modal dialogs: owns the open state, the centered panel
    // rect, and the dim backdrop. Subclasses implement their own content and input.
    class ModalDialog : public sky::ui::UIElement {
    public:
        void Open();
        void Close();
        bool IsOpen() const
        {
            return isOpen;
        }

    protected:
        // Centers a panel of preferred size within the bounds, clamped to a
        // minimum and the available space (a `margin` gap on each side).
        sky::ui::UIRect CenteredPanel(float preferredWidth, float preferredHeight, float minWidth, float minHeight, float margin = 30.0f) const;
        void            PaintBackdrop(sky::ui::UIPaintContext &context) const;

        bool isOpen = false;
    };

} // namespace sky::editor
