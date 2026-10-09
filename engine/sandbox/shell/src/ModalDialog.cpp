//
// Created on 2026/10/06.
//

#include <editor/shell/ModalDialog.h>

#include <editor/shell/UiDraw.h>
#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    void ModalDialog::Open()
    {
        isOpen = true;
        SetVisible(true);
        MarkPaintDirty();
    }

    void ModalDialog::Close()
    {
        isOpen = false;
        SetVisible(false);
        capturedElement = nullptr;
        MarkPaintDirty();
        if (onClosed) {
            onClosed();
        }
    }

    void ModalDialog::AddContentElement(sky::ui::UIElement *element)
    {
        if (element != nullptr) {
            contentElements.push_back(element);
        }
    }

    sky::ui::UIElement *ModalDialog::ContentAt(float x, float y) const
    {
        for (auto *element : contentElements) {
            if (element != nullptr && element->IsVisible() && element->GetBounds().Contains(x, y)) {
                return element;
            }
        }
        return nullptr;
    }

    sky::ui::UIEventResult ModalDialog::RouteContentPointer(const sky::ui::UIPointerEvent &event)
    {
        // A child that started a drag on DOWN keeps receiving MOVE/UP until release (pointer capture),
        // so a release outside its bounds clears the drag and never leaks to another child.
        if (capturedElement != nullptr) {
            capturedElement->OnPointerEvent(event);
            if (event.action == sky::ui::UIPointerAction::UP) {
                capturedElement = nullptr;
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        // A child that currently owns the pointer beyond its bounds (open dropdown/popup) is delivered first.
        for (auto *element : contentElements) {
            if (element != nullptr && element->IsVisible() && element->WantsPointerCapture() &&
                element->OnPointerEvent(event) == sky::ui::UIEventResult::HANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        if (sky::ui::UIElement *element = ContentAt(event.x, event.y)) {
            if (event.action == sky::ui::UIPointerAction::DOWN) {
                capturedElement = element;
            }
            return element->OnPointerEvent(event);
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult ModalDialog::RouteContentKey(const sky::ui::UIKeyEvent &event)
    {
        if (capturedElement != nullptr) {
            return capturedElement->OnKeyEvent(event);
        }
        for (auto *element : contentElements) {
            if (element != nullptr && element->IsVisible() && element->OnKeyEvent(event) == sky::ui::UIEventResult::HANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult ModalDialog::RouteContentText(const sky::ui::UITextInputEvent &event)
    {
        for (auto *element : contentElements) {
            if (element != nullptr && element->IsVisible() && element->OnTextInput(event) == sky::ui::UIEventResult::HANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIRect ModalDialog::CenteredPanel(float preferredWidth, float preferredHeight, float minWidth, float minHeight, float margin) const
    {
        const sky::ui::UIRect bounds = GetBounds();
        const float           w      = std::min(preferredWidth, std::max(minWidth, bounds.Width() - margin * 2.0f));
        const float           h      = std::min(preferredHeight, std::max(minHeight, bounds.Height() - margin * 2.0f));
        const float           left   = bounds.left + (bounds.Width() - w) * 0.5f;
        const float           top    = bounds.top + (bounds.Height() - h) * 0.5f;
        return sky::ui::UIRect{left, top, left + w, top + h};
    }

    void ModalDialog::PaintBackdrop(sky::ui::UIPaintContext &context) const
    {
        uc::Fill(context, GetBounds(), uc::RGB(0, 0, 0, 170));
    }

} // namespace sky::editor
