//
// Created on 2026/10/06.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
} // namespace sky::ui

namespace sky::editor {

    // Base for engine-drawn modal dialogs: owns the open state, the centered panel
    // rect, the dim backdrop, and (optionally) interactive child widgets.
    //
    // The shell routes input to the modal element only (it does not hit-test modal children), so a dialog
    // that embeds child widgets must call RouteContentPointer/Key/Text from its own On* overrides. Those
    // helpers add pointer capture (a drag started on a child keeps MOVE/UP until release) and overlay
    // priority (a child that currently owns the pointer, e.g. an open dropdown, is delivered first).
    class ModalDialog : public sky::ui::UIElement {
    public:
        void Open();
        void Close();
        bool IsOpen() const
        {
            return isOpen;
        }

        // Notified after the dialog closes (e.g. so the host clears its persisted open state).
        void SetOnClosed(std::function<void()> callback)
        {
            onClosed = std::move(callback);
        }

        // Registers a child that should receive raw input before the dialog's own handling.
        void AddContentElement(sky::ui::UIElement *element);

    protected:
        // Centers a panel of preferred size within the bounds, clamped to a
        // minimum and the available space (a `margin` gap on each side).
        sky::ui::UIRect CenteredPanel(float preferredWidth, float preferredHeight, float minWidth, float minHeight, float margin = 30.0f) const;
        void            PaintBackdrop(sky::ui::UIPaintContext &context) const;

        // Route an input event to the registered content children. Returns HANDLED when a child consumed
        // it; otherwise UNHANDLED (the dialog should handle it).
        sky::ui::UIEventResult RouteContentPointer(const sky::ui::UIPointerEvent &event);
        sky::ui::UIEventResult RouteContentKey(const sky::ui::UIKeyEvent &event);
        sky::ui::UIEventResult RouteContentText(const sky::ui::UITextInputEvent &event);
        sky::ui::UIElement    *ContentAt(float x, float y) const;

        bool                              isOpen = false;
        std::vector<sky::ui::UIElement *> contentElements;
        sky::ui::UIElement               *capturedElement = nullptr;
        std::function<void()>             onClosed;
    };

} // namespace sky::editor
