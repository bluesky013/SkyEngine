//
// Created on 2026/10/07.
//

#include <editor/shell/widgets/ToolBar.h>

#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    ToolBar::ToolBar(sky::ui::UITextSystem *text) : textSystem(text)
    {
    }

    void ToolBar::SetItems(std::vector<Item> inItems)
    {
        items     = std::move(inItems);
        hoverItem = -1;
        MarkPaintDirty();
    }

    void ToolBar::SetItemEnabled(const std::string &id, bool enabled)
    {
        for (Item &item : items) {
            if (item.id == id && item.enabled != enabled) {
                item.enabled = enabled;
                MarkPaintDirty();
            }
        }
    }

    void ToolBar::SetItemIcon(const std::string &id, sky::ui::UITextureId texture)
    {
        for (Item &item : items) {
            if (item.id == id && item.icon != texture) {
                item.icon = texture;
                MarkPaintDirty();
            }
        }
    }

    float ToolBar::ItemWidth(size_t index) const
    {
        const UiTheme   &th = GetDefaultUiTheme();
        const UiMetrics &m  = th.metrics;
        float            w  = 2.0f * m.controlPad;
        if (items[index].icon != sky::ui::UI_INVALID_TEXTURE) {
            w += m.iconSize; // icon-only; the label is reserved for a future tooltip
        } else if (!items[index].label.empty()) {
            w += uc::TextWidth(items[index].label, th.fonts.label, textSystem);
        }
        return std::max(w, m.iconButtonWidth);
    }

    sky::ui::UIRect ToolBar::ItemRect(size_t index) const
    {
        const sky::ui::UIRect b = GetBounds();
        const UiMetrics      &m = GetDefaultUiTheme().metrics;
        const float           y = b.top + (b.Height() - m.frameHeight) * 0.5f;

        float x = b.left + m.padX;
        for (size_t i = 0; i <= index && i < items.size(); ++i) {
            if (i > 0) {
                x += m.itemSpacing;
                if (items[i].separatorBefore) {
                    x += m.itemSpacing;
                }
            }
            if (i == index) {
                return sky::ui::UIRect{x, y, x + ItemWidth(i), y + m.frameHeight};
            }
            x += ItemWidth(i);
        }
        return sky::ui::UIRect{};
    }

    int ToolBar::ItemAt(float x, float y) const
    {
        for (size_t i = 0; i < items.size(); ++i) {
            if (ItemRect(i).Contains(x, y)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void ToolBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme        &th = GetDefaultUiTheme();
        const UiMetrics      &m  = th.metrics;
        const sky::ui::UIRect b  = GetBounds();

        uc::Fill(context, b, th.colors.toolbar);
        uc::HLine(context, b.left, b.right, b.bottom - 1.0f, th.colors.border);

        for (size_t i = 0; i < items.size(); ++i) {
            const Item           &item = items[i];
            const sky::ui::UIRect r    = ItemRect(i);
            if (item.separatorBefore && i > 0) {
                // Full-height divider between toolbar groups (UE/Blender-style).
                const float lx = r.left - m.itemSpacing * 0.75f;
                uc::Fill(context, sky::ui::UIRect{lx, b.top + m.hairline, lx + 1.0f, b.bottom - m.hairline}, th.colors.border);
            }

            const bool     hovered = static_cast<int>(i) == hoverItem && item.enabled;
            const uint32_t color   = item.enabled ? th.colors.text : th.colors.textDisabled;
            if (hovered) {
                uc::RoundedRect(context, r, th.colors.rowHover, m.buttonRadius);
            }

            const float cy = (r.top + r.bottom) * 0.5f;
            if (item.icon != sky::ui::UI_INVALID_TEXTURE) {
                const float           iconLeft = (r.left + r.right) * 0.5f - m.iconSize * 0.5f;
                const sky::ui::UIRect iconRect{iconLeft, cy - m.iconSize * 0.5f, iconLeft + m.iconSize, cy + m.iconSize * 0.5f};
                context.AddTexturedQuad(iconRect, sky::ui::UIRect{0.0f, 0.0f, 1.0f, 1.0f}, item.icon, color);
            } else if (!item.label.empty()) {
                uc::Text(context, item.label, th.fonts.label, sky::ui::UIRect{r.left + m.controlPad, r.top, r.right - m.controlPad, r.bottom}, color,
                         textSystem, uc::HAlign::Center, uc::VAlign::Middle, true);
            }
        }

        // Hover tooltip: an icon-only item reveals its label below the bar (the
        // toolbar is not clipped to its bounds, like the MenuBar popup).
        if (hoverItem >= 0 && hoverItem < static_cast<int>(items.size())) {
            const Item &item = items[static_cast<size_t>(hoverItem)];
            if (item.icon != sky::ui::UI_INVALID_TEXTURE && !item.label.empty()) {
                const sky::ui::UIRect hovered = ItemRect(static_cast<size_t>(hoverItem));
                const float           w       = uc::TextWidth(item.label, th.fonts.label, textSystem) + 2.0f * m.controlPad;
                const sky::ui::UIRect tip{hovered.left, b.bottom + m.panelGap, hovered.left + w, b.bottom + m.panelGap + m.popupItemHeight};
                uc::Fill(context, tip, th.colors.borderSoft);
                uc::Fill(context, sky::ui::UIRect{tip.left + 1.0f, tip.top + 1.0f, tip.right - 1.0f, tip.bottom - 1.0f}, th.colors.panel);
                uc::Text(context, item.label, th.fonts.label, tip, th.colors.text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, true);
            }
        }
    }

    sky::ui::UIEventResult ToolBar::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const int hover = ItemAt(event.x, event.y);
            if (hover != hoverItem) {
                hoverItem = hover;
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        const int index = ItemAt(event.x, event.y);
        if (index >= 0) {
            Item &item = items[static_cast<size_t>(index)];
            if (item.enabled && item.action) {
                item.action();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

} // namespace sky::editor
