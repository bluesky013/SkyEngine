//
// Created on 2026/10/07.
//

#include <editor/widgets/ToolBar.h>

#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/UiSkin.h>

#include <ui/UIPaintContext.h>
#include <ui/UITooltip.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    ToolBar::ToolBar(sky::ui::UITextSystem *text) : textSystem(text)
    {
    }

    void ToolBar::SetItems(std::vector<Item> inItems)
    {
        items = std::move(inItems);
        ++itemsGeneration;
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

    bool ToolBar::RefreshActionStates()
    {
        if (refreshingState)
            return false;
        const std::weak_ptr<void> alive = lifetime;
        refreshingState                 = true;
        struct RefreshGuard {
            std::weak_ptr<void> alive;
            bool               *busy;
            ~RefreshGuard()
            {
                if (!alive.expired())
                    *busy = false;
            }
        } guard{alive, &refreshingState};
        const uint64_t generation = itemsGeneration;
        auto           refreshed  = items;
        bool           changed    = false;
        for (auto &item : refreshed) {
            if (!item.resolveState)
                continue;
            const auto resolve = item.resolveState;
            const auto state   = resolve();
            if (alive.expired() || generation != itemsGeneration)
                return false;
            changed |=
                item.enabled != state.enabled || item.visible != state.visible || item.checkable != state.checkable || item.checked != state.checked;
            item.enabled   = state.enabled;
            item.visible   = state.visible;
            item.checkable = state.checkable;
            item.checked   = state.checked;
        }
        if (changed)
            items = std::move(refreshed);
        if (hoverItem >= 0 && !items[static_cast<size_t>(hoverItem)].visible)
            hoverItem = -1;
        if (changed)
            MarkPaintDirty();
        return true;
    }

    float ToolBar::ItemWidth(size_t index) const
    {
        const UiTheme   &th = GetDefaultUiTheme();
        const UiMetrics &m  = th.metrics;
        float            w  = 2.0f * m.controlPad;
        if (!tabStyle && items[index].icon != sky::ui::UI_INVALID_TEXTURE) {
            w += m.iconSize;
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

        float x        = b.left + m.padX;
        bool  previous = false;
        for (size_t i = 0; i <= index && i < items.size(); ++i) {
            if (!items[i].visible)
                continue;
            if (previous) {
                x += m.itemSpacing;
                if (items[i].separatorBefore) {
                    x += m.itemSpacing;
                }
            }
            if (i == index) {
                return sky::ui::UIRect{x, y, x + ItemWidth(i), y + m.frameHeight};
            }
            x += ItemWidth(i);
            previous = true;
        }
        return sky::ui::UIRect{};
    }

    int ToolBar::ItemAt(float x, float y) const
    {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].visible && ItemRect(i).Contains(x, y)) {
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
        UiSkin                skin(th, textSystem);

        uc::Fill(context, b, th.colors.toolbar);
        uc::HLine(context, b.left, b.right, b.bottom - 1.0f, th.colors.border);

        bool previous = false;
        for (size_t i = 0; i < items.size(); ++i) {
            const Item &item = items[i];
            if (!item.visible)
                continue;
            const sky::ui::UIRect r = ItemRect(i);
            if (item.separatorBefore && previous) {
                // Full-height divider between toolbar groups (UE/Blender-style).
                const float lx = r.left - m.itemSpacing * 0.75f;
                uc::Fill(context, sky::ui::UIRect{lx, b.top + m.hairline, lx + 1.0f, b.bottom - m.hairline}, th.colors.border);
            }

            const bool     hovered = static_cast<int>(i) == hoverItem && item.enabled;
            const uint32_t color   = item.enabled ? th.colors.text : th.colors.textDisabled;
            if (tabStyle) {
                skin.DrawTab(context, r, item.label, item.checked, hovered);
                previous = true;
                continue;
            }
            skin.DrawToolItem(context, r, item.icon == sky::ui::UI_INVALID_TEXTURE ? item.label : std::string{}, hovered, item.enabled,
                              item.checkable && item.checked);

            const float cy = (r.top + r.bottom) * 0.5f;
            if (item.icon != sky::ui::UI_INVALID_TEXTURE) {
                const float           iconLeft = (r.left + r.right) * 0.5f - m.iconSize * 0.5f;
                const sky::ui::UIRect iconRect{iconLeft, cy - m.iconSize * 0.5f, iconLeft + m.iconSize, cy + m.iconSize * 0.5f};
                skin.DrawIcon(context, iconRect, item.icon, color);
            }
            previous = true;
        }
    }

    bool ToolBar::QueryTooltip(float x, float y, sky::ui::UITooltipTarget &out) const
    {
        const auto index = ItemAt(x, y);
        if (tabStyle || index < 0 || items[static_cast<size_t>(index)].label.empty())
            return false;
        out = {(itemsGeneration << 32) | (static_cast<uint32_t>(index) + 1), ItemRect(static_cast<size_t>(index)), GetInputReference().lifetime};
        return true;
    }

    bool ToolBar::BuildTooltip(uint64_t id, std::string &out) const
    {
        const auto index = static_cast<uint32_t>(id);
        if (tabStyle || (id >> 32) != itemsGeneration || !index || index > items.size() || !items[index - 1].visible ||
            items[index - 1].label.empty())
            return false;
        out = items[index - 1].label;
        return true;
    }

    sky::ui::UIEventResult ToolBar::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!RefreshActionStates())
            return sky::ui::UIEventResult::UNHANDLED;
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
            Item      &item   = items[static_cast<size_t>(index)];
            const auto action = item.action;
            if (item.enabled && action) {
                action();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

} // namespace sky::editor
