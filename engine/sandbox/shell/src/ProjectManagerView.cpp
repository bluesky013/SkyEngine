//
// Created on 2026/10/05.
//

#include <editor/shell/ProjectManagerView.h>

#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/UIRect.h>
#include <ui/text/UITextSystem.h>

#include <chrono>
#include <filesystem>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }

        std::string Stem(const std::string &path)
        {
            return std::filesystem::path(path).stem().string();
        }
    } // namespace

    void ProjectManagerView::SetRecent(std::vector<std::string> paths)
    {
        recent = std::move(paths);
        if (selected >= static_cast<int>(recent.size())) {
            selected = -1;
        }
        hoverRow      = -1;
        confirmDelete = false;
        MarkPaintDirty();
    }

    sky::ui::UIRect ProjectManagerView::TopButtonRect(int index) const
    {
        const sky::ui::UIRect b      = GetBounds();
        const UiMetrics      &m      = M();
        const float           totalW = m.hubButtonWidth * 3.0f + m.itemSpacing * 2.0f;
        const float           startX = b.right - m.dialogMargin - totalW;
        const float           left   = startX + static_cast<float>(index) * (m.hubButtonWidth + m.itemSpacing);
        const float           top    = b.top + (m.bannerHeight - m.frameHeight) * 0.5f;
        return sky::ui::UIRect{left, top, left + m.hubButtonWidth, top + m.frameHeight};
    }

    sky::ui::UIRect ProjectManagerView::ActionRect(int index) const
    {
        const sky::ui::UIRect b    = GetBounds();
        const UiMetrics      &m    = M();
        const float           left = b.left + m.dialogMargin + static_cast<float>(index) * (m.hubActionWidth + m.itemSpacing);
        const float           top  = b.top + m.bannerHeight + (m.hubActionBarHeight - m.frameHeight) * 0.5f;
        return sky::ui::UIRect{left, top, left + m.hubActionWidth, top + m.frameHeight};
    }

    sky::ui::UIRect ProjectManagerView::RowRect(int index) const
    {
        const sky::ui::UIRect b       = GetBounds();
        const UiMetrics      &m       = M();
        const float           rowsTop = b.top + m.bannerHeight + m.hubActionBarHeight + m.hubHeaderHeight;
        const float           top     = rowsTop + static_cast<float>(index) * m.hubRowHeight;
        return sky::ui::UIRect{b.left + m.dialogMargin, top, b.right - m.dialogMargin, top + m.hubRowHeight - m.checkboxPad * 2.0f};
    }

    int ProjectManagerView::TopButtonAt(float x, float y) const
    {
        for (int i = 0; i < 3; ++i) {
            if (TopButtonRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    int ProjectManagerView::ActionAt(float x, float y) const
    {
        for (int i = 0; i < 3; ++i) {
            if (ActionRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    int ProjectManagerView::RowAt(float x, float y) const
    {
        for (size_t i = 0; i < recent.size(); ++i) {
            if (RowRect(static_cast<int>(i)).Contains(x, y)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void ProjectManagerView::OnPaint(sky::ui::UIPaintContext &context)
    {
        const sky::ui::UIRect b  = GetBounds();
        const UiTheme        &th = GetDefaultUiTheme();
        const UiMetrics      &m  = th.metrics;
        uc::Fill(context, b, uc::color::Window);

        // Title banner.
        uc::Fill(context, sky::ui::UIRect{b.left, b.top, b.right, b.top + m.bannerHeight}, uc::color::Header);
        uc::Text(context, "SkyEngine  -  Project Manager", th.fonts.banner,
                 sky::ui::UIRect{b.left + m.dialogMargin, b.top, b.right - m.dialogMargin, b.top + m.bannerHeight}, uc::color::Text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, false);

        const char *topLabels[3] = {"Add Project...", "New Project", "Quit"};
        for (int i = 0; i < 3; ++i) {
            const sky::ui::UIRect btn = TopButtonRect(i);
            const uint32_t        bg  = i == hoverTop ? uc::color::AccentHover : uc::color::Accent;
            uc::RoundedField(context, btn, bg, uc::color::Border, m.buttonRadius);
            uc::Text(context, topLabels[i], th.fonts.label, btn, uc::color::White, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
        }
        uc::HLine(context, b.left, b.right, b.top + m.bannerHeight, uc::color::Border);

        // Action bar (acts on the selected project).
        const bool hasSelection = selected >= 0 && selected < static_cast<int>(recent.size());
        uc::Fill(context, sky::ui::UIRect{b.left, b.top + m.bannerHeight, b.right, b.top + m.bannerHeight + m.hubActionBarHeight},
                 uc::color::Section);
        const char *actLabels[3] = {"Open", "Remove from list", confirmDelete ? "Confirm delete" : "Delete folder"};
        for (int i = 0; i < 3; ++i) {
            const sky::ui::UIRect btn    = ActionRect(i);
            const bool            danger = i == 2 && confirmDelete;
            uint32_t              bg     = hasSelection ? (danger ? uc::color::Accent : uc::color::FieldHover) : uc::color::Section;
            if (hasSelection && i == hoverAction) {
                bg = danger ? uc::color::AccentHover : uc::color::RowHover;
            }
            const uint32_t text = hasSelection ? uc::color::Text : uc::color::TextDisabled;
            uc::RoundedField(context, btn, bg, uc::color::Border, m.buttonRadius);
            uc::Text(context, actLabels[i], th.fonts.label, btn, text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
        }
        uc::HLine(context, b.left, b.right, b.top + m.bannerHeight + m.hubActionBarHeight, uc::color::Border);

        // Header + status.
        const float headerTop = b.top + m.bannerHeight + m.hubActionBarHeight;
        uc::Text(context, "Recent Projects", th.fonts.section,
                 sky::ui::UIRect{b.left + m.dialogMargin, headerTop, b.right - m.dialogMargin, headerTop + m.hubHeaderHeight}, uc::color::TextMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        if (!status.empty()) {
            uc::Text(context, status, th.fonts.label,
                     sky::ui::UIRect{b.left + m.dialogMargin, headerTop, b.right - m.dialogMargin, headerTop + m.hubHeaderHeight}, uc::color::Text,
                     textSystem, uc::HAlign::Right, uc::VAlign::Middle, true);
        }

        if (recent.empty()) {
            uc::Text(context, "(no recent projects - click Add or New Project)", th.fonts.label,
                     sky::ui::UIRect{b.left + m.dialogMargin, headerTop + m.hubHeaderHeight, b.right - m.dialogMargin,
                                     headerTop + m.hubHeaderHeight + m.hubRowHeight},
                     uc::color::TextDisabled, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        }

        for (size_t i = 0; i < recent.size(); ++i) {
            const int             idx = static_cast<int>(i);
            const sky::ui::UIRect row = RowRect(idx);
            uint32_t              bg  = (idx % 2 == 0) ? uc::color::RowEven : uc::color::RowOdd;
            if (idx == selected) {
                bg = uc::color::RowSelected;
            } else if (idx == hoverRow) {
                bg = uc::color::RowHover;
            }
            uc::Fill(context, row, bg);

            const float inset = m.controlPad;
            const float nameW = (row.right - row.left) * 0.4f;
            uc::Text(context, Stem(recent[i]), th.fonts.label, sky::ui::UIRect{row.left + inset, row.top, row.left + nameW, row.bottom},
                     uc::color::Text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
            uc::Text(context, recent[i], th.fonts.label, sky::ui::UIRect{row.left + nameW, row.top, row.right - inset, row.bottom},
                     uc::color::TextMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    sky::ui::UIEventResult ProjectManagerView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const int row = RowAt(event.x, event.y);
            const int top = TopButtonAt(event.x, event.y);
            const int act = ActionAt(event.x, event.y);
            if (row != hoverRow || top != hoverTop || act != hoverAction) {
                hoverRow    = row;
                hoverTop    = top;
                hoverAction = act;
                MarkPaintDirty();
            }
            return (row >= 0 || top >= 0 || act >= 0) ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const int top = TopButtonAt(event.x, event.y);
        if (top == 0) {
            if (onAdd) {
                onAdd();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (top == 1) {
            if (onNew) {
                onNew();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (top == 2) {
            if (onQuit) {
                onQuit();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        const int act = ActionAt(event.x, event.y);
        if (act >= 0) {
            if (selected < 0 || selected >= static_cast<int>(recent.size())) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const std::string &path = recent[static_cast<size_t>(selected)];
            if (act == 0) {
                if (onOpen) {
                    onOpen(path);
                }
            } else if (act == 1) {
                confirmDelete = false;
                selected      = -1;
                if (onRemove) {
                    onRemove(path);
                }
            } else if (act == 2) {
                if (confirmDelete) {
                    confirmDelete = false;
                    selected      = -1;
                    if (onDelete) {
                        onDelete(path);
                    }
                } else {
                    confirmDelete = true;
                    SetStatus("Click 'Confirm delete' to permanently delete the project folder");
                }
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        const int row = RowAt(event.x, event.y);
        if (row >= 0) {
            using clock            = std::chrono::steady_clock;
            const auto now         = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
            const bool doubleClick = (row == selected && now - lastClickMs < 400);
            lastClickMs            = now;
            if (doubleClick && onOpen) {
                onOpen(recent[static_cast<size_t>(row)]);
            } else {
                selected      = row;
                confirmDelete = false;
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

} // namespace sky::editor
