//
// Created on 2026/10/06.
//

#include <editor/shell/widgets/MenuBar.h>

#include <editor/shell/UiDraw.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    MenuBar::MenuBar(std::vector<Menu> inMenus, sky::ui::UITextSystem *text) : menus(std::move(inMenus)), textSystem(text)
    {
    }

    float MenuBar::LabelWidth(const std::string &text) const
    {
        const UiMetrics &m = GetDefaultUiTheme().metrics;
        return 2.0f * m.padX + uc::TextWidth(text, GetDefaultUiTheme().fonts.label, textSystem);
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
        const float      rows = static_cast<float>(menus[static_cast<size_t>(openIndex)].items.size());
        return 2.0f * m.panelGap + RowHeight() * rows;
    }

    static float ItemsWidth(const std::vector<MenuBar::Item> &items, const UiTheme &th, sky::ui::UITextSystem *text)
    {
        float w = th.metrics.controlButtonWidth;
        for (const auto &item : items) {
            w = std::max(w, 3.0f * th.metrics.controlPad + uc::TextWidth(item.label, th.fonts.label, text));
        }
        return w;
    }

    void MenuBar::PaintPopup(
        sky::ui::UIPaintContext &context, const std::vector<Item> &items, const sky::ui::UIRect &popup, int32_t hovered, bool withSubmenuArrows) const
    {
        const UiTheme   &th   = GetDefaultUiTheme();
        const UiMetrics &m    = th.metrics;
        const float      rowH = RowHeight();

        uc::Fill(context, popup, th.colors.borderSoft);
        uc::Fill(context, sky::ui::UIRect{popup.left + 1.0f, popup.top + 1.0f, popup.right - 1.0f, popup.bottom - 1.0f}, th.colors.panel);
        UiSkin skin(th, textSystem);
        for (size_t i = 0; i < items.size(); ++i) {
            const float           top = popup.top + m.panelGap + rowH * static_cast<float>(i);
            const sky::ui::UIRect row{popup.left + 1.0f, top, popup.right - 1.0f, top + rowH};
            skin.DrawToolItem(context, row, items[i].label, static_cast<int32_t>(i) == hovered);
            if (withSubmenuArrows && !items[i].children.empty()) {
                skin.DrawTriangle(context, row.right - m.controlPad, (row.top + row.bottom) * 0.5f, true, th.colors.textMuted);
            }
        }
    }

    void MenuBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme        &th = GetDefaultUiTheme();
        const sky::ui::UIRect b  = GetBounds();
        const sky::ui::UIRect bar{b.left, b.top, b.right, b.top + barHeight};
        uc::Fill(context, bar, th.colors.toolbar);
        uc::HLine(context, bar.left, bar.right, bar.bottom - 1.0f, th.colors.borderSoft);
        UiSkin skin(th, textSystem);
        for (size_t i = 0; i < menus.size(); ++i) {
            skin.DrawToolItem(context, LabelRect(i), menus[i].label, static_cast<int32_t>(i) == openIndex || static_cast<int32_t>(i) == hoverLabel);
        }
        if (openIndex < 0) {
            return;
        }
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        PaintPopup(context, items, PopupRect(), hoverItem, true);

        // Second-level submenu for the hovered item.
        if (hoverItem >= 0 && hoverItem < static_cast<int32_t>(items.size()) && !items[static_cast<size_t>(hoverItem)].children.empty()) {
            PaintPopup(context, items[static_cast<size_t>(hoverItem)].children, SubmenuRect(), hoverSubItem, false);
        }
    }

    sky::ui::UIEventResult MenuBar::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            hoverLabel   = LabelAt(event.x, event.y);
            hoverItem    = ItemAt(event.x, event.y);
            hoverSubItem = -1;
            if (openIndex >= 0 && hoverItem >= 0) {
                const auto &items = menus[static_cast<size_t>(openIndex)].items;
                if (hoverItem < static_cast<int32_t>(items.size()) && !items[static_cast<size_t>(hoverItem)].children.empty()) {
                    hoverSubItem = SubmenuItemAt(event.x, event.y);
                }
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const int32_t label = LabelAt(event.x, event.y);
        if (label >= 0) {
            openIndex = (openIndex == label) ? -1 : label;
            hoverItem = hoverSubItem = -1;
            return sky::ui::UIEventResult::HANDLED;
        }
        if (openIndex < 0) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const auto &items = menus[static_cast<size_t>(openIndex)].items;

        // Submenu click.
        if (hoverItem >= 0 && hoverItem < static_cast<int32_t>(items.size()) && !items[static_cast<size_t>(hoverItem)].children.empty()) {
            const int32_t sub = SubmenuItemAt(event.x, event.y);
            if (sub >= 0) {
                auto action = items[static_cast<size_t>(hoverItem)].children[static_cast<size_t>(sub)].action;
                openIndex = hoverItem = hoverSubItem = -1;
                if (action) {
                    action();
                }
                return sky::ui::UIEventResult::HANDLED;
            }
        }

        const int32_t item = ItemAt(event.x, event.y);
        if (item >= 0) {
            if (!items[static_cast<size_t>(item)].children.empty()) {
                return sky::ui::UIEventResult::HANDLED; // opens on hover; ignore the click
            }
            auto action = items[static_cast<size_t>(item)].action;
            openIndex = hoverItem = hoverSubItem = -1;
            if (action) {
                action();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        openIndex = hoverItem = hoverSubItem = -1;
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIRect MenuBar::LabelRect(size_t index) const
    {
        const sky::ui::UIRect b = GetBounds();
        float                 x = b.left + GetDefaultUiTheme().metrics.panelGap;
        for (size_t i = 0; i < index; ++i) {
            x += LabelWidth(menus[i].label);
        }
        return sky::ui::UIRect{x, b.top, x + LabelWidth(menus[index].label), b.top + barHeight};
    }

    sky::ui::UIRect MenuBar::PopupRect() const
    {
        const sky::ui::UIRect label = LabelRect(static_cast<size_t>(openIndex));
        const float           w     = ItemsWidth(menus[static_cast<size_t>(openIndex)].items, GetDefaultUiTheme(), textSystem);
        return sky::ui::UIRect{label.left, label.bottom, label.left + w, label.bottom + PopupHeight()};
    }

    sky::ui::UIRect MenuBar::SubmenuRect() const
    {
        const UiTheme        &th    = GetDefaultUiTheme();
        const UiMetrics      &m     = th.metrics;
        const sky::ui::UIRect popup = PopupRect();
        const float           rowH  = RowHeight();
        const float           top   = popup.top + m.panelGap + rowH * static_cast<float>(hoverItem);
        const auto           &sub   = menus[static_cast<size_t>(openIndex)].items[static_cast<size_t>(hoverItem)].children;
        const float           w     = ItemsWidth(sub, th, textSystem);
        const float           h     = 2.0f * m.panelGap + rowH * static_cast<float>(sub.size());
        return sky::ui::UIRect{popup.right, top, popup.right + w, top + h};
    }

    int32_t MenuBar::LabelAt(float x, float y) const
    {
        const sky::ui::UIRect b = GetBounds();
        if (y < b.top || y > b.top + barHeight) {
            return -1;
        }
        for (size_t i = 0; i < menus.size(); ++i) {
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
        const int32_t index = static_cast<int32_t>((y - popup.top - m.panelGap) / RowHeight());
        const auto   &items = menus[static_cast<size_t>(openIndex)].items;
        if (index < 0 || index >= static_cast<int32_t>(items.size())) {
            return -1;
        }
        return index;
    }

    int32_t MenuBar::SubmenuItemAt(float x, float y) const
    {
        if (openIndex < 0 || hoverItem < 0) {
            return -1;
        }
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        if (hoverItem >= static_cast<int32_t>(items.size()) || items[static_cast<size_t>(hoverItem)].children.empty()) {
            return -1;
        }
        const UiMetrics      &m    = GetDefaultUiTheme().metrics;
        const sky::ui::UIRect rect = SubmenuRect();
        if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) {
            return -1;
        }
        const int32_t index = static_cast<int32_t>((y - rect.top - m.panelGap) / RowHeight());
        const auto   &sub   = items[static_cast<size_t>(hoverItem)].children;
        if (index < 0 || index >= static_cast<int32_t>(sub.size())) {
            return -1;
        }
        return index;
    }

} // namespace sky::editor
