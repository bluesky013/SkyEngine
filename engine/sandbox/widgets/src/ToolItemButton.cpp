//
// ToolItemButton implementation (see ToolItemButton.h).
//

#include <editor/widgets/ToolItemButton.h>

#include <editor/ui/UiDraw.h>
#include <editor/widgets/UiSkin.h>
#include <editor/ui/UiTheme.h>

#include <ui/UIPaintContext.h>

#include <utility>

namespace sky::editor {

    namespace {
        constexpr float kHeight = 22.0f;
    } // namespace

    ToolItemButton::ToolItemButton(UiSkin *skinPtr, std::string text, std::function<void()> callback)
        : skin(skinPtr), label(std::move(text)), onClick(std::move(callback))
    {
    }

    void ToolItemButton::Measure(float &outWidth, float &outHeight)
    {
        float pad = 8.0f;
        if (skin != nullptr) {
            pad = skin->Theme().metrics.controlPad;
        }
        if (hasIcon) {
            const float iconSize = skin != nullptr ? skin->Theme().metrics.iconSize : 16.0f;
            outWidth             = iconSize + 2.0f * pad;
            outHeight            = iconSize + 2.0f * 4.0f;
            return;
        }
        float textW = 0.0f;
        if (skin != nullptr) {
            textW = uidraw::TextWidth(label, skin->Theme().fonts.label, skin->Text());
        }
        outWidth  = textW + 2.0f * pad;
        outHeight = kHeight;
    }

    void ToolItemButton::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (skin == nullptr) {
            return;
        }
        if (!hasIcon) {
            skin->DrawToolItem(context, GetBounds(), label, hovered && IsEnabled(), IsEnabled());
            return;
        }
        if (hovered && IsEnabled()) {
            skin->DrawToolItem(context, GetBounds(), "", true);
        }
        const sky::ui::UIRect bounds   = GetBounds();
        const float           iconSize = skin->Theme().metrics.iconSize;
        const float           cx       = (bounds.left + bounds.right) * 0.5f;
        const float           cy       = (bounds.top + bounds.bottom) * 0.5f;
        const sky::ui::UIRect iconRect{cx - iconSize * 0.5f, cy - iconSize * 0.5f, cx + iconSize * 0.5f, cy + iconSize * 0.5f};
        skin->DrawIcon(context, iconRect, icon, IsEnabled() ? skin->Theme().colors.text : skin->Theme().colors.textMuted);
    }

    void ToolItemButton::OnPointerCancel(uint32_t pointerId)
    {
        (void)pointerId;
        pressed = false;
        hovered = false;
        MarkPaintDirty();
    }

    sky::ui::UIEventResult ToolItemButton::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!IsEnabled()) {
            OnPointerCancel(event.pointerId);
            return sky::ui::UIEventResult::HANDLED;
        }
        switch (event.action) {
        case sky::ui::UIPointerAction::DOWN:
            pressed = true;
            hovered = true;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        case sky::ui::UIPointerAction::UP: {
            const bool inside = pressed && GetBounds().Contains(event.x, event.y);
            pressed           = false;
            hovered           = inside;
            MarkPaintDirty();
            if (inside && onClick) {
                onClick();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        case sky::ui::UIPointerAction::MOVE:
            if (!pressed) {
                hovered = GetBounds().Contains(event.x, event.y);
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        default: return sky::ui::UIEventResult::UNHANDLED;
        }
    }

} // namespace sky::editor
