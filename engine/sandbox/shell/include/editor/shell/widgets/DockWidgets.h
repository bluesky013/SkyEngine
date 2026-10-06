//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/layout/DockInteraction.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIPaintContext.h>
#include <ui/UIRect.h>
#include <ui/text/UITextSystem.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sky::editor {

    namespace uc = uidraw;

    // Dock geometry constants shared by the shell and its dock widgets.
    inline constexpr float kTabHeaderH        = 26.0f;
    inline constexpr float kSplitterThickness = 6.0f;

    // Tab header row: one title per panel plus a close box; clicking a title
    // switches the active panel; dragging a title drives tab dock.
    class TabHeader : public sky::ui::UIElement {
    public:
        using DragFn = std::function<void(float, float)>;
        using PressFn = std::function<void(const std::string &, float, float)>;

        TabHeader(std::vector<std::string> inTitles, std::vector<std::string> inPanelIds, int32_t active,
                  sky::ui::UITextSystem *text, std::function<void(int32_t)> onSelect,
                  std::function<void(int32_t)> onClose, PressFn onPress, DragFn onMove, DragFn onRelease)
            : titles(std::move(inTitles))
            , panelIds(std::move(inPanelIds))
            , activeIndex(active)
            , textSystem(text)
            , select(std::move(onSelect))
            , close(std::move(onClose))
            , press(std::move(onPress))
            , move(std::move(onMove))
            , release(std::move(onRelease))
        {
        }

        const char *GetTypeName() const override { return "TabHeader"; }

        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            const UiTheme &th = GetDefaultUiTheme();
            const sky::ui::UIRect b = GetBounds();
            uc::Fill(context, b, th.colors.tabInactive);
            uc::HLine(context, b.left, b.right, b.bottom - 1.0f, th.colors.borderSoft);
            if (titles.empty()) {
                return;
            }
            UiSkin skin(th, textSystem);
            const float span = std::max(1.0f, b.right - b.left);
            const float cell = span / static_cast<float>(titles.size());
            for (size_t i = 0; i < titles.size(); ++i) {
                const float left = b.left + cell * static_cast<float>(i);
                const sky::ui::UIRect cellRect{left, b.top, left + cell, b.bottom};
                skin.DrawTab(context, cellRect, titles[i], static_cast<int32_t>(i) == activeIndex,
                             static_cast<int32_t>(i) == hoverIndex);
                const float closeX = cellRect.right - 14.0f;
                uc::Text(context, "x", 12, sky::ui::UIRect{closeX, b.top, closeX + 10.0f, b.bottom}, th.colors.text,
                         textSystem);
            }
        }

        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            if (event.action == sky::ui::UIPointerAction::DOWN) {
                if (titles.empty()) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                const int32_t index = IndexAt(event.x);
                if (index < 0) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                if (InCloseBox(event.x, index)) {
                    if (close) {
                        close(index);
                    }
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (select) {
                    select(index);
                }
                if (press && index < static_cast<int32_t>(panelIds.size())) {
                    press(panelIds[static_cast<size_t>(index)], event.x, event.y);
                }
                pressed = true;
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.action == sky::ui::UIPointerAction::MOVE) {
                hoverIndex = IndexAt(event.x);
                MarkPaintDirty();
                if (pressed && move) {
                    move(event.x, event.y);
                }
                return pressed ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
            }
            if (event.action == sky::ui::UIPointerAction::UP) {
                if (pressed && release) {
                    release(event.x, event.y);
                }
                pressed = false;
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

        void OnPointerLeave(const sky::ui::UIPointerEvent & /*event*/) override
        {
            hoverIndex = -1;
            MarkPaintDirty();
        }

    private:
        int32_t IndexAt(float x) const
        {
            if (titles.empty()) {
                return -1;
            }
            const sky::ui::UIRect b = GetBounds();
            const float t = (x - b.left) / std::max(1.0f, b.right - b.left);
            const int32_t index = static_cast<int32_t>(t * static_cast<float>(titles.size()));
            return std::clamp(index, 0, static_cast<int32_t>(titles.size()) - 1);
        }

        bool InCloseBox(float x, int32_t index) const
        {
            const sky::ui::UIRect b = GetBounds();
            const float span = std::max(1.0f, b.right - b.left);
            const float cell = span / static_cast<float>(titles.size());
            const float left = b.left + cell * static_cast<float>(index);
            return x >= left + cell - 18.0f;
        }

        std::vector<std::string>     titles;
        std::vector<std::string>     panelIds;
        int32_t                      activeIndex;
        sky::ui::UITextSystem       *textSystem;
        std::function<void(int32_t)> select;
        std::function<void(int32_t)> close;
        PressFn                      press;
        DragFn                       move;
        DragFn                       release;
        bool                         pressed = false;
        int32_t                      hoverIndex = -1;
    };

    // Thin grab handle on a split seam; drives SetRatio while dragged.
    class SplitterHandle : public sky::ui::UIElement {
    public:
        using DragFn = std::function<void(const SplitterBand &, float, float)>;

        const char *GetTypeName() const override { return "SplitterHandle"; }
        // True when the seam is vertical (a horizontal split) -> left/right resize.
        bool IsHorizontal() const { return band.orientation == SplitOrientation::HORIZONTAL; }
        void SetBand(const SplitterBand &inBand) { band = inBand; }
        void SetOnDrag(DragFn callback) { onDrag = std::move(callback); }
        void SetOnDragEnd(std::function<void()> callback) { onDragEnd = std::move(callback); }

        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            const UiTheme &th = GetDefaultUiTheme();
            const sky::ui::UIRect b = GetBounds();
            const bool active = dragging || hovered;
            const bool horizontal = band.orientation == SplitOrientation::HORIZONTAL;
            const float cx = (b.left + b.right) * 0.5f;
            const float cy = (b.top + b.bottom) * 0.5f;

            // Grab band: reveals the hit area on hover/drag (kept subtle).
            if (active) {
                uc::Fill(context, b, dragging ? 0x332E7CD6u : 0x1F2E7CD6u);
            }

            // Seam line (understated, UE-like).
            const uint32_t line = dragging ? th.colors.accentSoft : (hovered ? th.colors.border : th.colors.borderSoft);
            const float half = active ? 1.0f : 0.5f;
            if (horizontal) {
                uc::Fill(context, sky::ui::UIRect{cx - half, b.top, cx + half, b.bottom}, line);
            } else {
                uc::Fill(context, sky::ui::UIRect{b.left, cy - half, b.right, cy + half}, line);
            }

            // Centered grip handle on hover/drag.
            if (active) {
                constexpr float kGripLen = 22.0f;
                constexpr float kGripThick = 3.0f;
                if (horizontal) {
                    uc::Fill(context,
                             sky::ui::UIRect{cx - kGripThick * 0.5f, cy - kGripLen * 0.5f, cx + kGripThick * 0.5f,
                                             cy + kGripLen * 0.5f},
                             th.colors.textMuted);
                } else {
                    uc::Fill(context,
                             sky::ui::UIRect{cx - kGripLen * 0.5f, cy - kGripThick * 0.5f, cx + kGripLen * 0.5f,
                                             cy + kGripThick * 0.5f},
                             th.colors.text);
                }
            }
        }

        void OnPointerEnter(const sky::ui::UIPointerEvent & /*event*/) override
        {
            hovered = true;
            MarkPaintDirty();
        }

        void OnPointerLeave(const sky::ui::UIPointerEvent & /*event*/) override
        {
            hovered = false;
            MarkPaintDirty();
        }

        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
        {
            if (event.action == sky::ui::UIPointerAction::MOVE) {
                if (dragging && onDrag) {
                    onDrag(band, event.x, event.y);
                }
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.action == sky::ui::UIPointerAction::DOWN) {
                dragging = true;
                MarkPaintDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (event.action == sky::ui::UIPointerAction::UP) {
                dragging = false;
                MarkPaintDirty();
                if (onDragEnd) {
                    onDragEnd();
                }
                return sky::ui::UIEventResult::HANDLED;
            }
            return sky::ui::UIEventResult::UNHANDLED;
        }

    private:
        SplitterBand          band;
        DragFn                onDrag;
        std::function<void()> onDragEnd;
        bool                  dragging = false;
        bool                  hovered = false;
    };

    // Transient overlay showing the candidate drop zone during a tab drag.
    class DropHighlight : public sky::ui::UIElement {
    public:
        const char *GetTypeName() const override { return "DropHighlight"; }
        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            uc::Fill(context, GetBounds(), 0x882E7CD6u);
        }
    };

    // Small floating label that follows the pointer while a tab is dragged.
    class DragGhost : public sky::ui::UIElement {
    public:
        explicit DragGhost(sky::ui::UITextSystem *text) : textSystem(text) {}

        const char *GetTypeName() const override { return "DragGhost"; }
        void SetText(std::string value) { text = std::move(value); }

        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            const UiTheme &th = GetDefaultUiTheme();
            const sky::ui::UIRect b = GetBounds();
            uc::Fill(context, b, 0xE0141414u);
            uc::Border(context, b, th.colors.accent);
            uc::Text(context, text, 12, sky::ui::UIRect{b.left + 8.0f, b.top, b.right - 8.0f, b.bottom}, th.colors.text,
                     textSystem);
        }

    private:
        std::string            text;
        sky::ui::UITextSystem *textSystem = nullptr;
    };

} // namespace sky::editor
