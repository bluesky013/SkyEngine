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
        MarkPaintDirty();
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
