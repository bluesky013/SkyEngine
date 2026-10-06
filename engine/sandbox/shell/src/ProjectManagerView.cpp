//
// Created on 2026/10/05.
//

#include <editor/shell/ProjectManagerView.h>

#include <editor/shell/UiDraw.h>
#include <ui/UIPaintContext.h>
#include <ui/UIRect.h>
#include <ui/text/UITextSystem.h>

#include <chrono>
#include <filesystem>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        constexpr float kMargin     = 16.0f;
        constexpr float kTitleH     = 64.0f;
        constexpr float kActionBarH = 40.0f;
        constexpr float kHeaderH    = 28.0f;
        constexpr float kRowH       = 32.0f;
        constexpr float kTopBtnW    = 110.0f;
        constexpr float kActBtnW    = 150.0f;
        constexpr float kGap        = 8.0f;
        constexpr uint32_t kTitleSize = 22;
        constexpr uint32_t kTextSize  = 14;

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
        hoverRow = -1;
        confirmDelete = false;
        MarkPaintDirty();
    }

    sky::ui::UIRect ProjectManagerView::TopButtonRect(int index) const
    {
        const sky::ui::UIRect b = GetBounds();
        const float totalW = kTopBtnW * 3.0f + kGap * 2.0f;
        const float startX = b.right - kMargin - totalW;
        const float left = startX + static_cast<float>(index) * (kTopBtnW + kGap);
        return sky::ui::UIRect{left, b.top + 18.0f, left + kTopBtnW, b.top + 46.0f};
    }

    sky::ui::UIRect ProjectManagerView::ActionRect(int index) const
    {
        const sky::ui::UIRect b = GetBounds();
        const float left = b.left + kMargin + static_cast<float>(index) * (kActBtnW + kGap);
        return sky::ui::UIRect{left, b.top + kTitleH + 7.0f, left + kActBtnW, b.top + kTitleH + kActionBarH - 7.0f};
    }

    sky::ui::UIRect ProjectManagerView::RowRect(int index) const
    {
        const sky::ui::UIRect b = GetBounds();
        const float rowsTop = b.top + kTitleH + kActionBarH + kHeaderH;
        const float top = rowsTop + static_cast<float>(index) * kRowH;
        return sky::ui::UIRect{b.left + kMargin, top, b.right - kMargin, top + kRowH - 4.0f};
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
        const sky::ui::UIRect b = GetBounds();
        uc::Fill(context, b, uc::color::Window);

        // Title bar.
        uc::Fill(context, sky::ui::UIRect{b.left, b.top, b.right, b.top + kTitleH}, uc::color::Header);
        uc::Text(context, "SkyEngine  -  Project Manager", kTitleSize,
                 sky::ui::UIRect{b.left + kMargin, b.top, b.right - kMargin, b.top + kTitleH}, uc::color::Text,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);

        const char *topLabels[3] = {"Add Project...", "New Project", "Quit"};
        for (int i = 0; i < 3; ++i) {
            const sky::ui::UIRect btn = TopButtonRect(i);
            const uint32_t bg = i == hoverTop ? uc::color::AccentHover : uc::color::Accent;
            uc::RoundedField(context, btn, bg, uc::color::Border, 4.0f);
            uc::Text(context, topLabels[i], 13, btn, uc::color::White, textSystem, uc::HAlign::Center,
                     uc::VAlign::Middle, false);
        }
        uc::HLine(context, b.left, b.right, b.top + kTitleH, uc::color::Border);

        // Action bar (acts on the selected project).
        const bool hasSelection = selected >= 0 && selected < static_cast<int>(recent.size());
        uc::Fill(context, sky::ui::UIRect{b.left, b.top + kTitleH, b.right, b.top + kTitleH + kActionBarH},
                 uc::color::Section);
        const char *actLabels[3] = {"Open", "Remove from list", confirmDelete ? "Confirm delete" : "Delete folder"};
        for (int i = 0; i < 3; ++i) {
            const sky::ui::UIRect btn = ActionRect(i);
            const bool danger = i == 2 && confirmDelete;
            uint32_t bg = hasSelection ? (danger ? uc::color::Accent : uc::color::FieldHover) : uc::color::Section;
            if (hasSelection && i == hoverAction) {
                bg = danger ? uc::color::AccentHover : uc::color::RowHover;
            }
            const uint32_t text = hasSelection ? uc::color::Text : uc::color::TextDisabled;
            uc::RoundedField(context, btn, bg, uc::color::Border, 4.0f);
            uc::Text(context, actLabels[i], 13, btn, text, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
        }
        uc::HLine(context, b.left, b.right, b.top + kTitleH + kActionBarH, uc::color::Border);

        // Header + status.
        const float headerTop = b.top + kTitleH + kActionBarH;
        uc::Text(context, "Recent Projects", kTextSize,
                 sky::ui::UIRect{b.left + kMargin, headerTop, b.right - kMargin, headerTop + kHeaderH},
                 uc::color::TextMuted, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        if (!status.empty()) {
            uc::Text(context, status, 13,
                     sky::ui::UIRect{b.left + kMargin, headerTop, b.right - kMargin, headerTop + kHeaderH},
                     uc::color::Text, textSystem, uc::HAlign::Right, uc::VAlign::Middle, true);
        }

        if (recent.empty()) {
            uc::Text(context, "(no recent projects - click Add or New Project)", kTextSize,
                     sky::ui::UIRect{b.left + kMargin, headerTop + kHeaderH, b.right - kMargin,
                                     headerTop + kHeaderH + kRowH},
                     uc::color::TextDisabled, textSystem, uc::HAlign::Left, uc::VAlign::Middle, false);
        }

        for (size_t i = 0; i < recent.size(); ++i) {
            const int idx = static_cast<int>(i);
            const sky::ui::UIRect row = RowRect(idx);
            uint32_t bg = (idx % 2 == 0) ? uc::color::RowEven : uc::color::RowOdd;
            if (idx == selected) {
                bg = uc::color::RowSelected;
            } else if (idx == hoverRow) {
                bg = uc::color::RowHover;
            }
            uc::Fill(context, row, bg);

            const float nameW = (row.right - row.left) * 0.4f;
            uc::Text(context, Stem(recent[i]), kTextSize,
                     sky::ui::UIRect{row.left + 8.0f, row.top, row.left + nameW, row.bottom}, uc::color::Text,
                     textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
            uc::Text(context, recent[i], kTextSize,
                     sky::ui::UIRect{row.left + nameW, row.top, row.right - 8.0f, row.bottom}, uc::color::TextMuted,
                     textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
    }

    sky::ui::UIEventResult ProjectManagerView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const int row = RowAt(event.x, event.y);
            const int top = TopButtonAt(event.x, event.y);
            const int act = ActionAt(event.x, event.y);
            if (row != hoverRow || top != hoverTop || act != hoverAction) {
                hoverRow = row;
                hoverTop = top;
                hoverAction = act;
                MarkPaintDirty();
            }
            return (row >= 0 || top >= 0 || act >= 0) ? sky::ui::UIEventResult::HANDLED
                                                      : sky::ui::UIEventResult::UNHANDLED;
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
                selected = -1;
                if (onRemove) {
                    onRemove(path);
                }
            } else if (act == 2) {
                if (confirmDelete) {
                    confirmDelete = false;
                    selected = -1;
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
            using clock = std::chrono::steady_clock;
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
            const bool doubleClick = (row == selected && now - lastClickMs < 400);
            lastClickMs = now;
            if (doubleClick && onOpen) {
                onOpen(recent[static_cast<size_t>(row)]);
            } else {
                selected = row;
                confirmDelete = false;
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

} // namespace sky::editor
