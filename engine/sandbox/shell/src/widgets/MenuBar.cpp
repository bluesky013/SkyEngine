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

    MenuBar::MenuBar(std::vector<Menu> inMenus, sky::ui::UITextSystem *text)
        : menus(std::move(inMenus))
        , textSystem(text)
    {
    }

    float MenuBar::PopupHeight() const
    {
        if (openIndex < 0 || openIndex >= static_cast<int32_t>(menus.size())) {
            return 0.0f;
        }
        const float rows = static_cast<float>(menus[static_cast<size_t>(openIndex)].items.size());
        return kPad + kRow * rows + kPad;
    }

    void MenuBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme &th = GetDefaultUiTheme();
        const sky::ui::UIRect b = GetBounds();
        const sky::ui::UIRect bar{b.left, b.top, b.right, b.top + barHeight};
        uc::Fill(context, bar, th.colors.toolbar);
        uc::HLine(context, bar.left, bar.right, bar.bottom - 1.0f, th.colors.borderSoft);
        UiSkin skin(th, textSystem);
        for (size_t i = 0; i < menus.size(); ++i) {
            skin.DrawToolItem(context, LabelRect(i), menus[i].label,
                              static_cast<int32_t>(i) == openIndex || static_cast<int32_t>(i) == hoverLabel);
        }
        if (openIndex < 0) {
            return;
        }
        const sky::ui::UIRect popup = PopupRect();
        uc::Fill(context, popup, th.colors.borderSoft);
        uc::Fill(context, sky::ui::UIRect{popup.left + 1.0f, popup.top + 1.0f, popup.right - 1.0f, popup.bottom - 1.0f},
                 th.colors.panel);
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        for (size_t i = 0; i < items.size(); ++i) {
            const float top = popup.top + kPad + kRow * static_cast<float>(i);
            skin.DrawToolItem(context, sky::ui::UIRect{popup.left + 1.0f, top, popup.right - 1.0f, top + kRow},
                              items[i].label, static_cast<int32_t>(i) == hoverItem);
        }
    }

    sky::ui::UIEventResult MenuBar::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            hoverLabel = LabelAt(event.x, event.y);
            hoverItem  = ItemAt(event.x, event.y);
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        const int32_t label = LabelAt(event.x, event.y);
        if (label >= 0) {
            openIndex = (openIndex == label) ? -1 : label;
            hoverItem = -1;
            return sky::ui::UIEventResult::HANDLED;
        }
        if (openIndex >= 0) {
            const int32_t item = ItemAt(event.x, event.y);
            if (item >= 0) {
                auto action = menus[static_cast<size_t>(openIndex)].items[static_cast<size_t>(item)].action;
                openIndex = -1;
                hoverItem = -1;
                if (action) {
                    action();
                }
            } else {
                openIndex = -1;
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    float MenuBar::LabelWidth(const std::string &text)
    {
        return 10.0f * static_cast<float>(text.size()) + 20.0f;
    }

    sky::ui::UIRect MenuBar::LabelRect(size_t index) const
    {
        const sky::ui::UIRect b = GetBounds();
        float x = b.left + 6.0f;
        for (size_t i = 0; i < index; ++i) {
            x += LabelWidth(menus[i].label);
        }
        return sky::ui::UIRect{x, b.top, x + LabelWidth(menus[index].label), b.top + barHeight};
    }

    sky::ui::UIRect MenuBar::PopupRect() const
    {
        const sky::ui::UIRect label = LabelRect(static_cast<size_t>(openIndex));
        float w = 140.0f;
        for (const auto &item : menus[static_cast<size_t>(openIndex)].items) {
            w = std::max(w, 10.0f * static_cast<float>(item.label.size()) + 28.0f);
        }
        return sky::ui::UIRect{label.left, label.bottom, label.left + w, label.bottom + PopupHeight()};
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
        const sky::ui::UIRect popup = PopupRect();
        if (x < popup.left || x >= popup.right || y < popup.top || y >= popup.bottom) {
            return -1;
        }
        const int32_t index = static_cast<int32_t>((y - popup.top - kPad) / kRow);
        const auto &items = menus[static_cast<size_t>(openIndex)].items;
        if (index < 0 || index >= static_cast<int32_t>(items.size())) {
            return -1;
        }
        return index;
    }

} // namespace sky::editor
