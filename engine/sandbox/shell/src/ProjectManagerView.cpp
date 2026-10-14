//
// Created on 2026/10/05.
//

#include <editor/shell/ProjectManagerView.h>

#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/UiSkin.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        constexpr const char *TopLabels[]    = {"Add Project...", "New Project", "Quit"};
        constexpr const char *ActionLabels[] = {"Open", "Remove from list", "Delete folder", "Confirm delete"};

        float Extent(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }

        bool HasArea(const sky::ui::UIRect &rect)
        {
            return rect.right > rect.left && rect.bottom > rect.top;
        }

        std::string Stem(const std::string &path)
        {
            return std::filesystem::path(path).stem().string();
        }

        std::unique_ptr<sky::ui::UIElement> Region()
        {
            auto region = std::make_unique<sky::ui::UIElement>();
            region->SetLayoutVolatile(false);
            return region;
        }

        void SetPadding(sky::ui::UIElement &element, float horizontal, float vertical)
        {
            auto layout        = element.GetLayout();
            layout.paddingLeft = layout.paddingRight = horizontal;
            layout.paddingTop = layout.paddingBottom = vertical;
            element.SetLayout(layout);
        }

        sky::ui::UIRect Inset(const sky::ui::UIRect &bounds, float horizontal)
        {
            sky::ui::UILayoutParams padding;
            padding.paddingLeft = padding.paddingRight = Extent(horizontal);
            return padding.ContentRect(bounds);
        }
    } // namespace

    struct ProjectManagerView::Composition {
        sky::ui::DockPanel                 *root          = nullptr;
        sky::ui::DockPanel                 *banner        = nullptr;
        sky::ui::UIElement                 *title         = nullptr;
        sky::ui::GridPanel                 *topButtons    = nullptr;
        sky::ui::DockPanel                 *actions       = nullptr;
        sky::ui::GridPanel                 *actionButtons = nullptr;
        sky::ui::UIElement                 *listHeader    = nullptr;
        sky::ui::UIElement                 *list          = nullptr;
        sky::ui::UIElement                 *status        = nullptr;
        std::array<sky::ui::UIElement *, 3> top{};
        std::array<sky::ui::UIElement *, 3> action{};
        int                                 topColumns    = 0;
        int                                 actionColumns = 0;
        float                               topMargin     = -1.0f;
        float                               actionMargin  = -1.0f;
    };

    ProjectManagerView::ProjectManagerView() : composition(std::make_unique<Composition>())
    {
        using namespace sky::ui;
        // This host keeps the conservative policy: the hub's UIContext may
        // observe the shared theme without an EditorShell pre-layout callback.
        auto &parts         = *composition;
        parts.root          = static_cast<DockPanel *>(AddChild(std::make_unique<DockPanel>()));
        parts.banner        = static_cast<DockPanel *>(parts.root->Add(std::make_unique<DockPanel>(), {DockSide::Top}));
        parts.topButtons    = static_cast<GridPanel *>(parts.banner->Add(std::make_unique<GridPanel>(), {DockSide::Right}));
        parts.title         = parts.banner->Add(Region());
        parts.actions       = static_cast<DockPanel *>(parts.root->Add(std::make_unique<DockPanel>(), {DockSide::Top}));
        parts.actionButtons = static_cast<GridPanel *>(parts.actions->Add(std::make_unique<GridPanel>(), {DockSide::Left}));
        parts.listHeader    = parts.root->Add(Region(), {DockSide::Top});
        parts.status        = parts.root->Add(Region(), {DockSide::Bottom});
        parts.list          = parts.root->Add(Region());
    }

    ProjectManagerView::~ProjectManagerView() = default;

    void ProjectManagerView::SetTextSystem(sky::ui::UITextSystem *text)
    {
        if (textSystem != text) {
            textSystem = text;
            MarkLayoutDirty();
        }
    }

    void ProjectManagerView::SetEngineVersion(std::string version)
    {
        engineVersion = std::move(version);
        MarkPaintDirty();
    }

    void ProjectManagerView::SetRecent(std::vector<std::string> paths)
    {
        recent = std::move(paths);
        if (selected >= static_cast<int>(recent.size())) {
            selected = -1;
        }
        hoverRow      = -1;
        confirmDelete = false;
        MarkLayoutDirty();
    }

    void ProjectManagerView::SynchronizeLayout()
    {
        if (layoutBusy) {
            return;
        }
        layoutBusy = true;
        using namespace sky::ui;
        auto       &parts   = *composition;
        const auto &theme   = GetDefaultUiTheme();
        const auto &metrics = theme.metrics;
        SynchronizeDefaultUiTheme(*this, themeRevision);
        const UISize available{Extent(GetArrangedSize().width), Extent(GetArrangedSize().height)};
        const float  horizontal  = std::min(Extent(metrics.dialogMargin), available.width * 0.5f);
        const float  innerWidth  = std::max(0.0f, available.width - 2.0f * horizontal);
        const float  gap         = Extent(metrics.itemSpacing);
        const float  vertical    = Extent(metrics.cellPadding);
        const float  frame       = Extent(metrics.frameHeight);
        const auto   buttonWidth = [&](const auto &labels, float minimum) {
            float width = Extent(minimum);
            for (const auto *label : labels) {
                width = std::max(width, uc::TextWidth(label, theme.fonts.label, textSystem) + 2.0f * Extent(metrics.controlPad));
            }
            return width + gap;
        };
        const float topCell       = buttonWidth(TopLabels, metrics.hubButtonWidth);
        const float actionCell    = buttonWidth(ActionLabels, metrics.hubActionWidth);
        const auto  columnsFor    = [innerWidth](float cell) { return innerWidth >= 3.0f * cell ? 3 : innerWidth >= 2.0f * cell ? 2 : 1; };
        const int   topColumns    = columnsFor(topCell);
        const int   actionColumns = columnsFor(actionCell);
        const int   topRows       = (3 + topColumns - 1) / topColumns;
        const int   actionRows    = (3 + actionColumns - 1) / actionColumns;
        const float topWidth      = std::min(innerWidth, topCell * static_cast<float>(topColumns));
        const float actionWidth   = std::min(innerWidth, actionCell * static_cast<float>(actionColumns));
        const float topHeight     = static_cast<float>(topRows) * (frame + gap) + 2.0f * vertical;
        const float titleHeight   = frame + 2.0f * vertical;
        const float titleWidth    = uc::TextWidth("SkyEngine - Project Manager", theme.fonts.banner, textSystem) + 2.0f * horizontal;
        const bool  compactBanner = innerWidth < topWidth + titleWidth;
        const float bannerHeight =
            std::min(available.height, std::max(Extent(metrics.bannerHeight), compactBanner ? topHeight + titleHeight : topHeight));
        const float topAllocationHeight = compactBanner ? std::min(topHeight, bannerHeight) : bannerHeight;
        const float topPaddingY         = std::min(vertical, topAllocationHeight * 0.5f);
        SetPadding(*parts.banner, horizontal, 0.0f);
        SetPadding(*parts.topButtons, 0.0f, topPaddingY);
        parts.banner->SetSlot(parts.topButtons, {compactBanner ? DockSide::Bottom : DockSide::Right, compactBanner ? topAllocationHeight : topWidth});
        float       remainingHeight = std::max(0.0f, available.height - bannerHeight);
        const float actionHeight =
            std::min(remainingHeight, std::max(Extent(metrics.hubActionBarHeight), static_cast<float>(actionRows) * (frame + gap) + 2.0f * vertical));
        remainingHeight -= actionHeight;
        const float headerHeight = std::min(remainingHeight, Extent(metrics.hubHeaderHeight));
        remainingHeight -= headerHeight;
        const float statusHeight   = std::min(remainingHeight, Extent(metrics.hubHeaderHeight));
        const float actionPaddingY = std::min(vertical, actionHeight * 0.5f);
        SetPadding(*parts.actions, horizontal, actionPaddingY);
        parts.actions->SetSlot(parts.actionButtons, {DockSide::Left, actionWidth});
        SetPadding(*parts.list, horizontal, 0.0f);
        parts.root->SetSlot(parts.banner, {DockSide::Top, bannerHeight});
        parts.root->SetSlot(parts.actions, {DockSide::Top, actionHeight});
        parts.root->SetSlot(parts.listHeader, {DockSide::Top, headerHeight});
        parts.root->SetSlot(parts.status, {DockSide::Bottom, statusHeight});
        const auto configureGrid = [&](GridPanel &grid, std::array<UIElement *, 3> &buttons, int columns, int rows, float width, float height,
                                       int &oldColumns, float &oldMargin) {
            const float margin = std::min({gap * 0.5f, width / (2.0f * static_cast<float>(columns)), height / (2.0f * static_cast<float>(rows))});
            if (oldColumns != columns || oldMargin != margin) {
                grid.ClearChildren();
                grid.SetGrid(rows, columns);
                for (int index = 0; index < 3; ++index) {
                    buttons[index] = grid.Add(Region(), {index / columns, index % columns, 1, 1, margin});
                }
                oldColumns = columns;
                oldMargin  = margin;
            }
        };
        configureGrid(*parts.topButtons, parts.top, topColumns, topRows, compactBanner ? innerWidth : topWidth,
                      std::max(0.0f, topAllocationHeight - 2.0f * topPaddingY), parts.topColumns, parts.topMargin);
        configureGrid(*parts.actionButtons, parts.action, actionColumns, actionRows, actionWidth,
                      std::max(0.0f, actionHeight - 2.0f * actionPaddingY), parts.actionColumns, parts.actionMargin);
        parts.root->ArrangeIfNeeded(GetBounds(), available);
        layoutBusy = false;
    }

    void ProjectManagerView::ArrangeChildren(const sky::ui::UIRect &)
    {
        SynchronizeLayout();
    }

    sky::ui::UIRect ProjectManagerView::BannerRect() const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return composition->banner->GetBounds();
    }

    sky::ui::UIRect ProjectManagerView::ListViewportRect() const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return composition->list->GetLayout().ContentRect(composition->list->GetBounds());
    }

    sky::ui::UIRect ProjectManagerView::StatusRect() const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return composition->status->GetBounds();
    }

    sky::ui::UIRect ProjectManagerView::TopButtonRect(int index) const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return index >= 0 && index < 3 ? composition->top[static_cast<std::size_t>(index)]->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect ProjectManagerView::ActionRect(int index) const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return index >= 0 && index < 3 ? composition->action[static_cast<std::size_t>(index)]->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect ProjectManagerView::RecentRowBounds(int index) const
    {
        if (index < 0 || index >= static_cast<int>(recent.size())) {
            return {};
        }
        const auto &metrics  = GetDefaultUiTheme().metrics;
        const auto  viewport = composition->list->GetLayout().ContentRect(composition->list->GetBounds());
        const float height   = Extent(metrics.hubRowHeight);
        const float top      = viewport.top + static_cast<float>(index) * height;
        return {viewport.left, top, viewport.right, top + std::max(0.0f, height - 2.0f * Extent(metrics.checkboxPad))};
    }

    sky::ui::UIRect ProjectManagerView::RowRect(int index) const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        return RecentRowBounds(index);
    }

    int ProjectManagerView::TopButtonAt(float x, float y) const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        for (int index = 0; index < 3; ++index) {
            const auto rect = composition->top[static_cast<std::size_t>(index)]->GetBounds();
            if (HasArea(rect) && rect.Contains(x, y)) {
                return index;
            }
        }
        return -1;
    }

    int ProjectManagerView::ActionAt(float x, float y) const
    {
        const_cast<ProjectManagerView *>(this)->SynchronizeLayout();
        for (int index = 0; index < 3; ++index) {
            const auto rect = composition->action[static_cast<std::size_t>(index)]->GetBounds();
            if (HasArea(rect) && rect.Contains(x, y)) {
                return index;
            }
        }
        return -1;
    }

    int ProjectManagerView::RowAt(float x, float y) const
    {
        const auto viewport = ListViewportRect();
        if (!HasArea(viewport) || !viewport.Contains(x, y)) {
            return -1;
        }
        for (std::size_t index = 0; index < recent.size(); ++index) {
            const auto row = RecentRowBounds(static_cast<int>(index));
            if (HasArea(row) && row.Contains(x, y)) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    void ProjectManagerView::OnPaint(sky::ui::UIPaintContext &context)
    {
        const auto &theme   = GetDefaultUiTheme();
        const auto &metrics = theme.metrics;
        auto       &parts   = *composition;
        UiSkin      skin(theme, textSystem);
        context.PushClip(GetBounds());
        uc::Fill(context, GetBounds(), theme.colors.window);
        const auto banner = parts.banner->GetBounds();
        if (HasArea(banner)) {
            skin.DrawPanelHeader(context, banner);
            const auto        title = Inset(parts.title->GetBounds(), metrics.padX);
            const std::string label = engineVersion.empty() ? "SkyEngine - Project Manager" : "SkyEngine " + engineVersion + " - Project Manager";
            uc::Text(context, label, theme.fonts.banner, title, theme.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        }
        for (int index = 0; index < 3; ++index) {
            if (parts.top[static_cast<std::size_t>(index)] != nullptr) {
                const auto rect = parts.top[static_cast<std::size_t>(index)]->GetBounds();
                if (HasArea(rect)) {
                    const bool enabled = !busy || index == 2;
                    skin.DrawToolItem(context, rect, TopLabels[index], enabled && index == hoverTop, enabled, true);
                }
            }
        }
        const auto actions = parts.actions->GetBounds();
        uc::Fill(context, actions, theme.colors.section);
        const bool hasSelection = !busy && selected >= 0 && selected < static_cast<int>(recent.size());
        for (int index = 0; index < 3; ++index) {
            if (parts.action[static_cast<std::size_t>(index)] != nullptr) {
                const auto rect = parts.action[static_cast<std::size_t>(index)]->GetBounds();
                if (HasArea(rect)) {
                    skin.DrawToolItem(context, rect, index == 2 && confirmDelete ? ActionLabels[3] : ActionLabels[index],
                                      hasSelection && index == hoverAction, hasSelection, index == 2 && confirmDelete);
                }
            }
        }
        uc::Text(context, "Recent Projects", theme.fonts.section, Inset(parts.listHeader->GetBounds(), metrics.dialogMargin), theme.colors.textMuted,
                 textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
        uc::Text(context, status, theme.fonts.label, Inset(parts.status->GetBounds(), metrics.dialogMargin), theme.colors.text, textSystem,
                 uc::HAlign::Left, uc::VAlign::Middle, true);
        const auto viewport = parts.list->GetLayout().ContentRect(parts.list->GetBounds());
        context.PushClip(viewport);
        if (recent.empty()) {
            uc::Text(context, "(no recent projects - click Add or New Project)", theme.fonts.label, viewport, theme.colors.textDisabled, textSystem,
                     uc::HAlign::Left, uc::VAlign::Top, true);
        }
        for (std::size_t index = 0; index < recent.size(); ++index) {
            const auto row = RecentRowBounds(static_cast<int>(index));
            if (row.bottom <= viewport.top || row.top >= viewport.bottom || !HasArea(row)) {
                continue;
            }
            skin.DrawRow(context, row,
                         static_cast<int>(index) == selected   ? RowState::Selected
                         : static_cast<int>(index) == hoverRow ? RowState::Hover
                         : index % 2                           ? RowState::Alt
                                                               : RowState::Normal);
            const auto  textRect = Inset(row, metrics.controlPad);
            const float split    = textRect.left + parts.list->GetContentSize().width * 0.4f;
            uc::Text(context, Stem(recent[index]), theme.fonts.label,
                     {textRect.left, row.top, std::clamp(split, textRect.left, textRect.right), row.bottom}, theme.colors.text, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
            uc::Text(context, recent[index], theme.fonts.label,
                     {std::clamp(split, textRect.left, textRect.right), row.top, textRect.right, row.bottom}, theme.colors.textMuted, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
        }
        context.PopClip();
        context.PopClip();
    }

    sky::ui::UIEventResult ProjectManagerView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        SynchronizeLayout();
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
            return row >= 0 || top >= 0 || act >= 0 ? sky::ui::UIEventResult::HANDLED : sky::ui::UIEventResult::UNHANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        const int top = TopButtonAt(event.x, event.y);
        if (busy && top != 2) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (top >= 0) {
            auto callback = top == 0 ? onAdd : top == 1 ? onNew : onQuit;
            if (callback) {
                callback();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        const int act = ActionAt(event.x, event.y);
        if (act >= 0) {
            if (selected < 0 || selected >= static_cast<int>(recent.size())) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const std::string path     = recent[static_cast<std::size_t>(selected)];
            auto              callback = act == 0 ? onOpen : act == 1 ? onRemove : onDelete;
            if (act == 1 || (act == 2 && confirmDelete)) {
                confirmDelete = false;
                selected      = -1;
                MarkPaintDirty();
            } else if (act == 2) {
                confirmDelete = true;
                SetStatus("Click 'Confirm delete' to permanently delete the project folder");
                return sky::ui::UIEventResult::HANDLED;
            }
            if (callback) {
                callback(path);
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        const int row = RowAt(event.x, event.y);
        if (row >= 0) {
            using clock            = std::chrono::steady_clock;
            const auto now         = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
            const bool doubleClick = row == selected && now - lastClickMs < 400;
            lastClickMs            = now;
            if (doubleClick && onOpen) {
                const auto path     = recent[static_cast<std::size_t>(row)];
                auto       callback = onOpen;
                callback(path);
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
