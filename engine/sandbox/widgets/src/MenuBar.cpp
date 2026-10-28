//
// Created on 2026/10/06.
//

#include <editor/widgets/MenuBar.h>

#include <editor/core/extension/IconRegistry.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/UiSkin.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    MenuBar::MenuBar(std::vector<Menu> inMenus, sky::ui::UITextSystem *text) : menus(std::move(inMenus)), textSystem(text)
    {
        RefreshActionStates();
    }

    void MenuBar::SetMenus(std::vector<Menu> inMenus, bool preserveOpen)
    {
        const std::string openLabel = preserveOpen && openIndex >= 0 ? menus[static_cast<size_t>(openIndex)].label : std::string{};
        menus                       = std::move(inMenus);
        ++menusGeneration;
        openIndex = hoverLabel = hoverItem = hoverSubItem = -1;
        if (!openLabel.empty()) {
            for (size_t i = 0; i < menus.size(); ++i) {
                if (menus[i].label == openLabel) {
                    openIndex = static_cast<int32_t>(i);
                    break;
                }
            }
        }
        MarkPaintDirty();
        RefreshActionStates();
    }

    size_t MenuBar::VisibleRowCount(const std::vector<Item> &items)
    {
        return static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const Item &item) { return item.visible; }));
    }

    bool MenuBar::MenuVisible(size_t index) const
    {
        return VisibleRowCount(menus[index].items) != 0;
    }

    bool MenuBar::RefreshActionStates()
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
        const uint64_t generation = menusGeneration;
        auto           refreshed  = menus;
        bool           changed    = false;
        const auto     refresh    = [&](const auto &self, std::vector<Item> &items) -> bool {
            for (auto &item : items) {
                EditorActionState state{item.enabled, item.visible, item.checkable, item.checked};
                if (item.resolveState) {
                    const auto resolve = item.resolveState;
                    state              = resolve();
                    if (alive.expired() || generation != menusGeneration)
                        return false;
                }
                if (!item.children.empty()) {
                    if (!self(self, item.children))
                        return false;
                    state.visible = std::any_of(item.children.begin(), item.children.end(), [](const Item &child) { return child.visible; });
                    state.enabled =
                        std::any_of(item.children.begin(), item.children.end(), [](const Item &child) { return child.visible && child.enabled; });
                }
                changed |= item.enabled != state.enabled || item.visible != state.visible || item.checkable != state.checkable ||
                           item.checked != state.checked;
                item.enabled   = state.enabled;
                item.visible   = state.visible;
                item.checkable = state.checkable;
                item.checked   = state.checked;
            }
            return true;
        };
        for (auto &menu : refreshed)
            if (!refresh(refresh, menu.items))
                return false;
        if (changed)
            menus = std::move(refreshed);
        if (openIndex >= 0) {
            if (!MenuVisible(static_cast<size_t>(openIndex))) {
                openIndex = hoverItem = hoverSubItem = -1;
            } else if (hoverItem >= 0) {
                const auto &items = menus[static_cast<size_t>(openIndex)].items;
                if (!items[static_cast<size_t>(hoverItem)].visible || !items[static_cast<size_t>(hoverItem)].enabled) {
                    hoverItem = hoverSubItem = -1;
                }
            }
        }
        if (hoverLabel >= 0 && !MenuVisible(static_cast<size_t>(hoverLabel)))
            hoverLabel = -1;
        if (changed)
            MarkPaintDirty();
        return true;
    }

    float MenuBar::LabelWidth(const std::string &text) const
    {
        const UiMetrics &m = GetDefaultUiTheme().metrics;
        return 2.0f * m.controlPad + uc::TextWidth(text, GetDefaultUiTheme().fonts.label, textSystem);
    }

    float MenuBar::RowHeight() const
    {
        return GetDefaultUiTheme().metrics.popupItemHeight;
    }

    float MenuBar::PopupHeight() const
    {
        if (openIndex < 0 || openIndex >= static_cast<int32_t>(menus.size())) {
            return 0.0f;
        }
        const UiMetrics &m    = GetDefaultUiTheme().metrics;
        const float      rows = static_cast<float>(VisibleRowCount(menus[static_cast<size_t>(openIndex)].items));
        return 2.0f * m.panelGap + RowHeight() * rows;
    }

    namespace {
        struct PopupColumns {
            float width         = 0.0f;
            float checkLeft     = 0.0f;
            float iconLeft      = 0.0f;
            float labelLeft     = 0.0f;
            float labelRight    = 0.0f;
            float shortcutLeft  = 0.0f;
            float shortcutRight = 0.0f;
            float arrowCenter   = 0.0f;
        };

        PopupColumns MeasurePopup(const std::vector<MenuBar::Item> &items, const UiTheme &theme, sky::ui::UITextSystem *text, bool arrows)
        {
            const UiMetrics &m     = theme.metrics;
            float            label = 0.0f, shortcut = 0.0f;
            bool             check = false, icon = false, arrow = false;
            for (const auto &item : items) {
                if (!item.visible || item.separator)
                    continue;
                label    = std::max(label, uc::TextWidth(item.label, theme.fonts.label, text));
                shortcut = std::max(shortcut, uc::TextWidth(item.shortcut, theme.fonts.small, text));
                check    = check || item.checkable;
                icon     = icon || !item.icon.empty();
                arrow    = arrow || (arrows && !item.children.empty());
            }
            PopupColumns columns;
            float        x    = m.controlPad;
            columns.checkLeft = x;
            if (check)
                x += m.iconSize + m.itemSpacing;
            columns.iconLeft = x;
            if (icon)
                x += m.iconSize + m.itemSpacing;
            columns.labelLeft = x;
            x += label;
            columns.labelRight = x;
            if (shortcut > 0.0f)
                x += m.itemSpacing;
            columns.shortcutLeft = x;
            x += shortcut;
            columns.shortcutRight = x;
            if (arrow)
                x += m.itemSpacing;
            columns.arrowCenter = x + m.iconSize * 0.5f;
            if (arrow)
                x += m.iconSize;
            columns.width     = std::max(m.controlButtonWidth, x + m.controlPad);
            const float extra = columns.width - x - m.controlPad;
            columns.labelRight += extra;
            columns.shortcutLeft += extra;
            columns.shortcutRight += extra;
            columns.arrowCenter += extra;
            return columns;
        }
    } // namespace

    void MenuBar::PaintPopup(
        sky::ui::UIPaintContext &context, const std::vector<Item> &items, const sky::ui::UIRect &popup, int32_t hovered, bool withSubmenuArrows) const
    {
        const UiTheme     &th      = GetDefaultUiTheme();
        const UiMetrics   &m       = th.metrics;
        const float        rowH    = RowHeight();
        const PopupColumns columns = MeasurePopup(items, th, textSystem, withSubmenuArrows);

        uc::Fill(context, popup, th.colors.borderSoft);
        uc::Fill(context, sky::ui::UIRect{popup.left + 1.0f, popup.top + 1.0f, popup.right - 1.0f, popup.bottom - 1.0f}, th.colors.panel);
        UiSkin skin(th, textSystem);
        size_t rowIndex = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const Item &item = items[i];
            if (!item.visible)
                continue;
            const float           top = popup.top + m.panelGap + rowH * static_cast<float>(rowIndex++);
            const sky::ui::UIRect row{popup.left + 1.0f, top, popup.right - 1.0f, top + rowH};
            if (item.separator) {
                const float y = (row.top + row.bottom) * 0.5f;
                uc::HLine(context, row.left + m.controlPad, row.right - m.controlPad, y, th.colors.borderSoft);
                continue;
            }
            const bool hoveredRow = static_cast<int32_t>(i) == hovered && item.enabled;
            if (hoveredRow) {
                uc::RoundedRect(context, row, th.colors.rowHover, m.buttonRadius);
            }
            const float    cy        = (row.top + row.bottom) * 0.5f;
            const uint32_t textColor = item.enabled ? th.colors.text : th.colors.textDisabled;
            if (item.checkable) {
                const float           cb = m.iconSize;
                const float           x  = popup.left + columns.checkLeft;
                const sky::ui::UIRect box{x, cy - cb * 0.5f, x + cb, cy + cb * 0.5f};
                skin.DrawCheckbox(context, box, item.checked, hoveredRow);
            }
            if (!item.icon.empty()) {
                const uint32_t iconId = IconRegistry::Get()->Find(item.icon);
                if (iconId != 0) {
                    const float           isz = m.iconSize;
                    const float           x   = popup.left + columns.iconLeft;
                    const sky::ui::UIRect ir{x, cy - isz * 0.5f, x + isz, cy + isz * 0.5f};
                    skin.DrawIcon(context, ir, iconId, textColor);
                }
            }
            uc::Text(context, item.label, th.fonts.label,
                     sky::ui::UIRect{popup.left + columns.labelLeft, row.top, popup.left + columns.labelRight, row.bottom}, textColor, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, false);
            if (!item.shortcut.empty()) {
                uc::Text(context, item.shortcut, th.fonts.small,
                         sky::ui::UIRect{popup.left + columns.shortcutLeft, row.top, popup.left + columns.shortcutRight, row.bottom},
                         item.enabled ? th.colors.textMuted : th.colors.textDisabled, textSystem, uc::HAlign::Right, uc::VAlign::Middle, false);
            }
            if (withSubmenuArrows && !item.children.empty()) {
                skin.DrawTriangle(context, popup.left + columns.arrowCenter, cy, true, item.enabled ? th.colors.textMuted : th.colors.textDisabled);
            }
        }
    }

    void MenuBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme        &th = GetDefaultUiTheme();
        const sky::ui::UIRect b  = GetBounds();
        const sky::ui::UIRect bar{b.left, b.top, b.right, b.top + barHeight};
        uc::Fill(context, bar, th.colors.window);
        uc::HLine(context, bar.left, bar.right, bar.bottom - 1.0f, th.colors.border);
        const UiMetrics &m = th.metrics;
        for (size_t i = 0; i < menus.size(); ++i) {
            if (!MenuVisible(i))
                continue;
            const sky::ui::UIRect r = LabelRect(i);
            const sky::ui::UIRect pill{r.left, r.top + m.panelGap, r.right, r.bottom - m.panelGap};
            const bool            on = static_cast<int32_t>(i) == openIndex || static_cast<int32_t>(i) == hoverLabel;
            if (on) {
                uc::RoundedRect(context, pill, th.colors.rowHover, m.buttonRadius);
            }
            uc::Text(context, menus[i].label, th.fonts.label, pill, th.colors.text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
        }
        if (openIndex < 0) {
            return;
        }
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        PaintPopup(context, items, PopupRect(), hoverItem, true);

        // Second-level submenu for the hovered item.
        if (hoverItem >= 0 && hoverItem < static_cast<int32_t>(items.size()) && items[static_cast<size_t>(hoverItem)].enabled &&
            !items[static_cast<size_t>(hoverItem)].children.empty()) {
            PaintPopup(context, items[static_cast<size_t>(hoverItem)].children, SubmenuRect(), hoverSubItem, false);
        }
    }

    sky::ui::UIEventResult MenuBar::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!RefreshActionStates())
            return sky::ui::UIEventResult::UNHANDLED;
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const int32_t sub = SubmenuItemAt(event.x, event.y);
            hoverLabel        = LabelAt(event.x, event.y);
            if (sub < 0)
                hoverItem = ItemAt(event.x, event.y);
            hoverSubItem = sub;
            MarkPaintDirty();
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const int32_t label = LabelAt(event.x, event.y);
        if (label >= 0) {
            openIndex = (openIndex == label) ? -1 : label;
            hoverItem = hoverSubItem = -1;
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (openIndex < 0) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const auto &items = menus[static_cast<size_t>(openIndex)].items;

        // Submenu click.
        if (hoverItem >= 0 && hoverItem < static_cast<int32_t>(items.size()) && items[static_cast<size_t>(hoverItem)].enabled &&
            !items[static_cast<size_t>(hoverItem)].children.empty()) {
            const int32_t sub = SubmenuItemAt(event.x, event.y);
            if (sub >= 0) {
                const auto &entry = items[static_cast<size_t>(hoverItem)].children[static_cast<size_t>(sub)];
                if (!entry.enabled || entry.separator)
                    return sky::ui::UIEventResult::HANDLED;
                auto action = entry.action;
                openIndex = hoverItem = hoverSubItem = -1;
                MarkPaintDirty();
                if (action) {
                    action();
                }
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        const int32_t item = ItemAt(event.x, event.y);
        if (item >= 0) {
            if (!items[static_cast<size_t>(item)].enabled || items[static_cast<size_t>(item)].separator)
                return sky::ui::UIEventResult::HANDLED;
            if (!items[static_cast<size_t>(item)].children.empty()) {
                return sky::ui::UIEventResult::HANDLED; // opens on hover; ignore the click
            }
            auto action = items[static_cast<size_t>(item)].action;
            openIndex = hoverItem = hoverSubItem = -1;
            MarkPaintDirty();
            if (action) {
                action();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        openIndex = hoverItem = hoverSubItem = -1;
        MarkPaintDirty();
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIRect MenuBar::LabelRect(size_t index) const
    {
        const sky::ui::UIRect b = GetBounds();
        const UiMetrics      &m = GetDefaultUiTheme().metrics;
        float                 x = b.left + m.padX;
        for (size_t i = 0; i < index; ++i) {
            if (MenuVisible(i))
                x += LabelWidth(menus[i].label) + m.itemSpacing;
        }
        return sky::ui::UIRect{x, b.top, x + LabelWidth(menus[index].label), b.top + barHeight};
    }

    sky::ui::UIRect MenuBar::PopupRect() const
    {
        const sky::ui::UIRect label = LabelRect(static_cast<size_t>(openIndex));
        const float           w     = MeasurePopup(menus[static_cast<size_t>(openIndex)].items, GetDefaultUiTheme(), textSystem, true).width;
        return sky::ui::UIRect{label.left, label.bottom, label.left + w, label.bottom + PopupHeight()};
    }

    sky::ui::UIRect MenuBar::SubmenuRect() const
    {
        const UiTheme        &th          = GetDefaultUiTheme();
        const UiMetrics      &m           = th.metrics;
        const sky::ui::UIRect popup       = PopupRect();
        const float           rowH        = RowHeight();
        const auto           &parentItems = menus[static_cast<size_t>(openIndex)].items;
        size_t                row         = 0;
        for (int32_t i = 0; i < hoverItem; ++i)
            if (parentItems[static_cast<size_t>(i)].visible)
                ++row;
        const float top = popup.top + m.panelGap + rowH * static_cast<float>(row);
        const auto &sub = menus[static_cast<size_t>(openIndex)].items[static_cast<size_t>(hoverItem)].children;
        const float w   = MeasurePopup(sub, th, textSystem, false).width;
        const float h   = 2.0f * m.panelGap + rowH * static_cast<float>(VisibleRowCount(sub));
        return sky::ui::UIRect{popup.right, top, popup.right + w, top + h};
    }

    int32_t MenuBar::LabelAt(float x, float y) const
    {
        const sky::ui::UIRect b = GetBounds();
        if (y < b.top || y > b.top + barHeight) {
            return -1;
        }
        for (size_t i = 0; i < menus.size(); ++i) {
            if (!MenuVisible(i))
                continue;
            const sky::ui::UIRect r = LabelRect(i);
            if (x >= r.left && x < r.right) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }

    int32_t MenuBar::ItemAt(float x, float y) const
    {
        if (openIndex < 0) {
            return -1;
        }
        const UiMetrics      &m     = GetDefaultUiTheme().metrics;
        const sky::ui::UIRect popup = PopupRect();
        if (x < popup.left || x >= popup.right || y < popup.top || y >= popup.bottom) {
            return -1;
        }
        if (y < popup.top + m.panelGap)
            return -1;
        int32_t     index = static_cast<int32_t>((y - popup.top - m.panelGap) / RowHeight());
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].visible && index-- == 0)
                return static_cast<int32_t>(i);
        return -1;
    }

    int32_t MenuBar::SubmenuItemAt(float x, float y) const
    {
        if (openIndex < 0 || hoverItem < 0) {
            return -1;
        }
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        if (hoverItem >= static_cast<int32_t>(items.size()) || !items[static_cast<size_t>(hoverItem)].enabled ||
            items[static_cast<size_t>(hoverItem)].children.empty()) {
            return -1;
        }
        const UiMetrics      &m    = GetDefaultUiTheme().metrics;
        const sky::ui::UIRect rect = SubmenuRect();
        if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) {
            return -1;
        }
        if (y < rect.top + m.panelGap)
            return -1;
        int32_t     index = static_cast<int32_t>((y - rect.top - m.panelGap) / RowHeight());
        const auto &sub   = items[static_cast<size_t>(hoverItem)].children;
        for (size_t i = 0; i < sub.size(); ++i)
            if (sub[i].visible && index-- == 0)
                return static_cast<int32_t>(i);
        return -1;
    }

} // namespace sky::editor
