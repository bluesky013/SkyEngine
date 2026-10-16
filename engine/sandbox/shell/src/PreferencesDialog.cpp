//
// Created on 2026/10/06.
//

#include <editor/shell/PreferencesDialog.h>

#include <editor/core/EditorCore.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/preferences/PreferencesPersistence.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ReflectedFormView.h>

#include <core/type/Rtti.h>
#include <framework/serialization/SerializationFactory.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/ScrollView.h>
#include <ui/widgets/VBox.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }

        struct PanelAllocation {
            sky::ui::UIRect bounds;
            sky::ui::UISize size;
        };

        PanelAllocation AllocatePanel(const sky::ui::UIElement &owner, const UiMetrics &metrics)
        {
            const auto            available = owner.GetArrangedSize();
            const float           margin    = std::max(0.0f, metrics.dialogMargin);
            const sky::ui::UISize size{std::min(std::max(0.0f, metrics.preferencesPanelWidth), std::max(0.0f, available.width - 2.0f * margin)),
                                       std::min(std::max(0.0f, metrics.preferencesPanelHeight), std::max(0.0f, available.height - 2.0f * margin))};
            const auto           &bounds = owner.GetBounds();
            const float           left   = bounds.left + (available.width - size.width) * 0.5f;
            const float           top    = bounds.top + (available.height - size.height) * 0.5f;
            return {{left, top, left + size.width, top + size.height}, size};
        }

        enum class PreferenceItemKind { Header, Category, Button };

        // Layout owns these rows; the shared skin owns their appearance. No
        // callbacks or model pointers survive a page/session replacement here.
        class PreferenceItem final : public sky::ui::UIElement {
        public:
            PreferenceItem(sky::ui::UITextSystem *text, std::string label, PreferenceItemKind kind)
                : skin(GetDefaultUiTheme(), text), label(std::move(label)), kind(kind)
            {
                SetLayoutVolatile(false);
                SetClipsChildren(true);
            }
            void SetLabel(std::string value)
            {
                if (label != value) {
                    label = std::move(value);
                    MarkLayoutDirty();
                }
            }
            void SetState(bool selected, bool hovered, bool alternate = false)
            {
                if (this->selected != selected || this->hovered != hovered || this->alternate != alternate) {
                    this->selected  = selected;
                    this->hovered   = hovered;
                    this->alternate = alternate;
                    MarkPaintDirty();
                }
            }
            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                const auto &rect = GetBounds();
                if (rect.Width() <= 0.0f || rect.Height() <= 0.0f) {
                    return;
                }
                const auto &theme = skin.Theme();
                context.PushClip(rect);
                if (kind == PreferenceItemKind::Header) {
                    skin.DrawPanelHeader(context, rect);
                    auto textRect = rect;
                    textRect.left += theme.metrics.padX;
                    textRect.right = std::max(textRect.left, textRect.right - theme.metrics.padX);
                    uc::Text(context, label, theme.fonts.title, textRect, theme.colors.textMuted, skin.Text(), uc::HAlign::Left, uc::VAlign::Middle,
                             true);
                } else if (kind == PreferenceItemKind::Category) {
                    skin.DrawRow(context, rect,
                                 selected    ? RowState::Selected
                                 : hovered   ? RowState::Hover
                                 : alternate ? RowState::Alt
                                             : RowState::Normal);
                    auto textRect = rect;
                    textRect.left = std::min(rect.right, rect.left + theme.metrics.padX);
                    uc::Text(context, label, theme.fonts.label, textRect, theme.colors.text, skin.Text(), uc::HAlign::Left, uc::VAlign::Middle, true);
                } else {
                    skin.DrawToolItem(context, rect, label, hovered, true, selected);
                }
                context.PopClip();
            }

        protected:
            sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &) override
            {
                const auto &theme = skin.Theme();
                return {uc::TextWidth(label, theme.fonts.label, skin.Text()) + 2.0f * theme.metrics.controlPad,
                        kind == PreferenceItemKind::Category ? theme.metrics.categoryRowHeight : theme.metrics.frameHeight};
            }

        private:
            UiSkin             skin;
            std::string        label;
            PreferenceItemKind kind;
            bool               selected  = false;
            bool               hovered   = false;
            bool               alternate = false;
        };

        bool HasArea(const sky::ui::UIRect &rect)
        {
            return rect.Width() > 0.0f && rect.Height() > 0.0f;
        }

        void SetPadding(sky::ui::UIElement &element, float horizontal, float vertical)
        {
            auto layout        = element.GetLayout();
            layout.paddingLeft = layout.paddingRight = horizontal;
            layout.paddingTop = layout.paddingBottom = vertical;
            element.SetLayout(layout);
        }

        constexpr const char *ButtonLabels[] = {"OK", "Cancel", "Apply", "Reset"};
    } // namespace

    struct PreferencesDialog::Layout {
        sky::ui::DockPanel             *panel          = nullptr;
        PreferenceItem                 *header         = nullptr;
        sky::ui::DockPanel             *body           = nullptr;
        sky::ui::DockPanel             *footer         = nullptr;
        sky::ui::GridPanel             *buttons        = nullptr;
        sky::ui::ScrollView            *categoryScroll = nullptr;
        sky::ui::VBox                  *categories     = nullptr;
        std::vector<PreferenceItem *>   rows;
        std::vector<std::string>        titles;
        std::array<PreferenceItem *, 4> buttonItems{};
        int                             columns      = 0;
        float                           buttonMargin = -1.0f;
    };

    PreferencesDialog::PreferencesDialog(sky::ui::UITextSystem *text)
        : composition(std::make_unique<Layout>()), textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        using namespace sky::ui;
        SetVisible(false);
        composition->panel = static_cast<DockPanel *>(AddChild(std::make_unique<DockPanel>()));
        // Category/form viewports clip their own rows. Keep popup painting
        // free to extend to the dialog's surface, matching ViewBounds().
        composition->header = static_cast<PreferenceItem *>(
            composition->panel->Add(std::make_unique<PreferenceItem>(text, "Preferences", PreferenceItemKind::Header), {DockSide::Top}));
        composition->footer         = static_cast<DockPanel *>(composition->panel->Add(std::make_unique<DockPanel>(), {DockSide::Bottom}));
        composition->buttons        = static_cast<GridPanel *>(composition->footer->Add(std::make_unique<GridPanel>(), {DockSide::Right}));
        composition->body           = static_cast<DockPanel *>(composition->panel->Add(std::make_unique<DockPanel>()));
        composition->categoryScroll = static_cast<ScrollView *>(composition->body->Add(std::make_unique<ScrollView>(), {DockSide::Left}));
        composition->categoryScroll->SetScrollAxes(ScrollAxes::Vertical);
        composition->categories = static_cast<VBox *>(composition->categoryScroll->SetContent(std::make_unique<VBox>()));
        UILayoutParams listLayout;
        listLayout.width.mode = UISizeMode::FILL;
        composition->categories->SetLayout(listLayout); // AUTO height may exceed the viewport.
        form = static_cast<ReflectedFormView *>(composition->body->Add(std::make_unique<ReflectedFormView>(
            text, "",
            ReflectedFormContext{session->History(), EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()})));
        form->SetHeaderVisible(false);
        form->SetVisible(false);
    }

    PreferencesDialog::~PreferencesDialog()
    {
        lifetime.reset();
        // UIElement destroys children after derived members. The form must
        // unsubscribe before the local session/history and working pages die.
        if (form != nullptr) {
            form->Unbind();
        }
        ClearChildren();
        form = nullptr;
    }

    bool PreferencesDialog::IsTransitionBusy() const
    {
        return transitioning || (form != nullptr && form->Form().IsEditing());
    }

    void PreferencesDialog::SetModel(PreferencePageRegistry *inRegistry)
    {
        const auto activeSession = session; // survive notification-triggered owner destruction
        if (IsTransitionBusy()) {
            pending         = Pending::Model;
            pendingRegistry = inRegistry;
            return;
        }
        const bool reopen = isOpen;
        transitioning     = true;
        ++generation;
        if (form != nullptr) {
            form->CancelInteractions();
            form->Unbind();
        }
        const std::weak_ptr<void> alive = lifetime;
        activeSession->End();
        if (alive.expired()) {
            return;
        }
        registry      = inRegistry;
        transitioning = false;
        if (reopen) {
            Open();
        }
    }

    int PreferencesDialog::GetPageCount() const
    {
        return isOpen ? static_cast<int>(session->GetPageCount()) : 0;
    }

    bool PreferencesDialog::IsEditable() const
    {
        return isOpen && pageIndex >= 0 && pageIndex < GetPageCount();
    }

    void PreferencesDialog::DrainPending()
    {
        if (IsTransitionBusy()) {
            return;
        }
        const auto action = pending;
        pending           = Pending::None;
        if (action == Pending::Open) {
            Open();
        } else if (action == Pending::Close) {
            Close();
        } else if (action == Pending::Model) {
            SetModel(pendingRegistry);
        }
    }

    void PreferencesDialog::Open()
    {
        const auto activeSession = session; // survive notification-triggered owner destruction
        if (IsTransitionBusy()) {
            pending = Pending::Open;
            return;
        }
        transitioning = true;
        ++generation;
        hoverCategory   = -1;
        hoverButton     = -1;
        lastApplyResult = {};
        composition->header->SetLabel("Preferences");
        formCaptured = false;
        composition->categoryScroll->SetScroll(0.0f, 0.0f);
        pageIndex = 0;
        if (form != nullptr) {
            form->CancelInteractions();
            form->Unbind();
        }
        const std::weak_ptr<void> alive = lifetime;
        activeSession->Begin(registry);
        if (alive.expired()) {
            return;
        }
        ModalDialog::Open();
        transitioning = false;
        if (form != nullptr) {
            form->SetVisible(true);
        }
        BindPage(pageIndex);
    }

    std::function<void()> PreferencesDialog::CloseState()
    {
        const auto activeSession = session; // survive notification-triggered owner destruction
        transitioning            = true;
        ++generation;
        if (form != nullptr) {
            form->CancelInteractions();
            form->Unbind();
            form->SetVisible(false);
        }
        const std::weak_ptr<void> alive = lifetime;
        activeSession->End();
        if (alive.expired()) {
            return {};
        }
        isOpen = false;
        SetVisible(false);
        capturedElement = nullptr;
        formCaptured    = false;
        MarkPaintDirty();
        transitioning = false;
        return onClosed;
    }

    void PreferencesDialog::Close()
    {
        if (IsTransitionBusy()) {
            pending = Pending::Close;
            return;
        }
        if (!isOpen) {
            return;
        }
        const auto closed = CloseState();
        if (closed) {
            closed(); // may destroy this; no member access follows
        }
    }

    void PreferencesDialog::SynchronizePages()
    {
        const auto activeSession = session; // survive notification-triggered owner destruction
        if (!isOpen || IsTransitionBusy() || activeSession->MatchesRegistry()) {
            return;
        }
        std::string selected;
        if (const auto *page = activeSession->GetPage(static_cast<std::size_t>(std::max(0, pageIndex)))) {
            selected = page->id;
        }
        transitioning = true;
        // This model is owned by the session, so cancellation is safe even
        // after a plugin has removed/replaced the external live registration.
        form->CancelInteractions();
        form->Unbind();
        const std::weak_ptr<void> alive = lifetime;
        activeSession->Synchronize();
        if (alive.expired()) {
            return;
        }
        transitioning = false;
        pageIndex     = 0;
        for (std::size_t i = 0; i < activeSession->GetPageCount(); ++i) {
            if (activeSession->GetPage(i)->id == selected) {
                pageIndex = static_cast<int>(i);
                break;
            }
        }
        BindPage(pageIndex);
        MarkLayoutDirty();
    }

    void PreferencesDialog::SetPageIndex(int index)
    {
        const std::weak_ptr<void> alive = lifetime;
        SynchronizePages();
        if (alive.expired()) {
            return;
        }
        if (IsTransitionBusy() || index < 0 || index >= GetPageCount()) {
            return;
        }
        pageIndex = index;
        BindPage(index);
        UpdatePresentation();
        MarkPaintDirty();
    }

    void PreferencesDialog::BindPage(int index)
    {
        if (form == nullptr) {
            return;
        }
        formCaptured = false;
        form->CancelInteractions();
        const auto *page = index >= 0 ? session->GetPage(static_cast<std::size_t>(index)) : nullptr;
        if (page == nullptr) {
            form->Unbind();
        } else {
            form->Bind(PropertyObject{page->object, page->type});
        }
        MarkPaintDirty();
    }

    void PreferencesDialog::Apply()
    {
        Commit(false);
    }

    void PreferencesDialog::Commit(bool close)
    {
        const auto activeSession = session; // survive notification-triggered owner destruction
        if (!isOpen || IsTransitionBusy()) {
            return;
        }
        const std::weak_ptr<void> synchronizeAlive = lifetime;
        SynchronizePages();
        if (synchronizeAlive.expired()) {
            return;
        }
        transitioning = true;
        form->CancelInteractions();
        form->Unbind();
        const std::weak_ptr<void> operationAlive      = lifetime;
        const auto                activePersistence   = persistence;
        const auto                operationGeneration = generation;
        const auto                result = activeSession->ApplyDetailed(activePersistence.get(), [this, operationAlive, operationGeneration] {
            return !operationAlive.expired() && operationGeneration == generation && pending == Pending::None;
        });
        if (operationAlive.expired()) {
            return;
        }
        transitioning = false;
        BindPage(pageIndex);
        if (operationAlive.expired()) {
            return;
        }
        lastApplyResult = result;
        if (!result.Succeeded()) {
            const std::string reason = result.message.empty() ? "Could not apply settings." : result.message;
            composition->header->SetLabel("Preferences: " + reason + (result.HasPartialEffects() ? " Changes may already be applied or saved." : ""));
            MarkPaintDirty();
            auto callback = onApplyResult;
            if (callback) {
                callback(result);
            }
            return;
        }
        composition->header->SetLabel("Preferences");
        const auto                callback = onApplied;
        const std::weak_ptr<void> alive    = lifetime;
        const auto                closed   = close ? CloseState() : std::function<void()>{};
        if (alive.expired()) {
            return;
        }
        const auto appliedGeneration = generation;
        const auto resultCallback    = onApplyResult;
        if (resultCallback) {
            resultCallback(result);
        }
        if (alive.expired() || appliedGeneration != generation) {
            return;
        }
        if (callback) {
            callback(); // may replace pages, reopen, close or destroy this
        }
        if (alive.expired()) {
            return;
        }
        if (closed && appliedGeneration == generation) {
            closed(); // do not notify an old close after the callback reopened
        }
    }

    void PreferencesDialog::ResetPage()
    {
        if (form == nullptr || IsTransitionBusy()) {
            return;
        }
        const auto                activeSession = session; // history/storage survive an observer deleting this
        const std::weak_ptr<void> alive         = lifetime;
        transitioning                           = true;
        form->CancelInteractions();
        ReflectedForm &reflected = form->Form();
        for (FormSection &section : reflected.GetSections()) {
            for (PropertyField &field : section.fields) {
                reflected.ResetToDefault(field, activeSession->History());
                if (alive.expired()) {
                    return;
                }
                if (pending != Pending::None) {
                    transitioning = false;
                    return;
                }
            }
        }
        transitioning = false;
        form->Refresh();
    }

    sky::ui::UIRect PreferencesDialog::ComputePanelRect() const
    {
        // The viewport is the hard limit, including when it is smaller than the
        // preferred dialog. Shared ModalDialog minimum behavior stays unchanged.
        return AllocatePanel(*this, M()).bounds;
    }

    void PreferencesDialog::UpdateComposition()
    {
        using namespace sky::ui;
        const auto              &m     = M();
        auto                    &parts = *composition;
        std::vector<std::string> titles;
        for (int i = 0; i < GetPageCount(); ++i) {
            titles.push_back(session->GetPage(static_cast<std::size_t>(i))->title);
        }
        if (titles != parts.titles) {
            parts.categories->ClearChildren();
            parts.rows.clear();
            for (const auto &title : titles) {
                parts.rows.push_back(static_cast<PreferenceItem *>(
                    parts.categories->AddChild(std::make_unique<PreferenceItem>(textSystem, title, PreferenceItemKind::Category))));
            }
            parts.titles = std::move(titles);
        }
        for (std::size_t i = 0; i < parts.rows.size(); ++i) {
            UILayoutParams row;
            row.width.mode = UISizeMode::FILL;
            row.height     = {UISizeMode::FIXED, std::max(0.0f, m.categoryRowHeight)};
            parts.rows[i]->SetLayout(row);
        }
        const auto   allocation    = AllocatePanel(*this, m);
        const UIRect panel         = allocation.bounds;
        const float  width         = allocation.size.width;
        const float  height        = allocation.size.height;
        const float  headerHeight  = std::min(std::max(0.0f, m.panelHeaderHeight), height);
        const float  horizontalPad = std::min(std::max(0.0f, m.dialogMargin), width * 0.5f);
        const float  innerWidth    = std::max(0.0f, width - 2.0f * horizontalPad);
        float        buttonWidth   = std::max(0.0f, m.buttonMinWidth);
        for (const auto *label : ButtonLabels) {
            buttonWidth = std::max(buttonWidth, uc::TextWidth(label, skin.Theme().fonts.label, textSystem) + 2.0f * m.controlPad);
        }
        const float gap       = std::max(0.0f, m.itemSpacing);
        const float cellWidth = buttonWidth + gap;
        const int   columns   = innerWidth >= 4.0f * cellWidth ? 4 : innerWidth >= 2.0f * cellWidth ? 2 : 1;
        const int   rows      = 4 / columns;
        const float footerHeight =
            std::min(std::max(std::max(0.0f, m.footerHeight),
                              static_cast<float>(rows) * (std::max(0.0f, m.frameHeight) + gap) + 2.0f * std::max(0.0f, m.cellPadding)),
                     std::max(0.0f, height - headerHeight));
        const float verticalPad = std::min(std::max(0.0f, m.cellPadding), footerHeight * 0.5f);
        const float gridWidth   = std::min(innerWidth, static_cast<float>(columns) * cellWidth);
        const float gridHeight  = std::max(0.0f, footerHeight - 2.0f * verticalPad);
        const float cellMargin  = std::min({gap * 0.5f, gridWidth / (2.0f * columns), gridHeight / (2.0f * rows)});
        if (parts.columns != columns || parts.buttonMargin != cellMargin) {
            parts.buttons->ClearChildren();
            parts.buttons->SetGrid(rows, columns);
            for (int order = 0; order < 4; ++order) {
                const int index          = 3 - order;
                parts.buttonItems[index] = static_cast<PreferenceItem *>(
                    parts.buttons->Add(std::make_unique<PreferenceItem>(textSystem, ButtonLabels[index], PreferenceItemKind::Button),
                                       {order / columns, order % columns, 1, 1, cellMargin}));
            }
            parts.columns      = columns;
            parts.buttonMargin = cellMargin;
        }
        UpdatePresentation();
        SetPadding(*parts.footer, horizontalPad, verticalPad);
        parts.footer->SetSlot(parts.buttons, {DockSide::Right, gridWidth});
        parts.panel->SetSlot(parts.header, {DockSide::Top, headerHeight});
        parts.panel->SetSlot(parts.footer, {DockSide::Bottom, footerHeight});
        const float bodyHeight = std::max(0.0f, height - headerHeight - footerHeight);
        const float bodyPadX   = std::min(std::max(0.0f, m.itemSpacing), width * 0.5f);
        const float bodyPadY   = std::min(std::max(0.0f, m.itemSpacing), bodyHeight * 0.5f);
        SetPadding(*parts.body, bodyPadX, bodyPadY);
        const float bodyWidth     = std::max(0.0f, width - 2.0f * bodyPadX);
        const float categoryWidth = std::min(std::max(0.0f, m.categoryWidth), bodyWidth * 0.5f);
        parts.body->SetSlot(parts.categoryScroll, {DockSide::Left, categoryWidth});
        auto categoryLayout         = parts.categoryScroll->GetLayout();
        categoryLayout.paddingRight = std::min(std::max(0.0f, m.scrollBarWidth), categoryWidth);
        parts.categoryScroll->SetLayout(categoryLayout);
        parts.panel->ArrangeIfNeeded(panel, {width, height});
    }

    void PreferencesDialog::UpdatePresentation()
    {
        for (std::size_t i = 0; i < composition->rows.size(); ++i) {
            composition->rows[i]->SetState(static_cast<int>(i) == pageIndex, static_cast<int>(i) == hoverCategory, i % 2 != 0);
        }
        for (int i = 0; i < 4; ++i) {
            if (composition->buttonItems[i] != nullptr) {
                composition->buttonItems[i]->SetState(i == 0, hoverButton == i);
            }
        }
    }

    void PreferencesDialog::SynchronizeLayout()
    {
        if (layoutBusy) {
            return;
        }
        layoutBusy                      = true;
        const std::weak_ptr<void> alive = lifetime;
        DrainPending();
        if (alive.expired()) {
            return;
        }
        SynchronizePages();
        if (alive.expired()) {
            return;
        }
        if (isOpen) {
            SynchronizeDefaultUiTheme(*this, themeRevision);
            form->SynchronizeLayout();
            if (alive.expired()) {
                return;
            }
            UpdateComposition();
        }
        layoutBusy = false;
    }

    sky::ui::UIRect PreferencesDialog::PanelRect() const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() ? ComputePanelRect() : sky::ui::UIRect{};
    }

    sky::ui::UIRect PreferencesDialog::ContentRect() const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() && isOpen ? composition->body->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect PreferencesDialog::CategoryViewportRect() const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() && isOpen ? composition->categoryScroll->GetLayout().ContentRect(composition->categoryScroll->GetBounds())
                                          : sky::ui::UIRect{};
    }

    sky::ui::UIRect PreferencesDialog::PageViewportRect() const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() && isOpen ? form->GetBounds() : sky::ui::UIRect{};
    }

    sky::ui::UIRect PreferencesDialog::CategoryRect(int index) const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() && isOpen && index >= 0 && index < static_cast<int>(composition->rows.size())
                   ? composition->rows[static_cast<std::size_t>(index)]->GetBounds()
                   : sky::ui::UIRect{};
    }

    int PreferencesDialog::CategoryAt(float x, float y) const
    {
        const std::weak_ptr<void> alive    = lifetime;
        const auto                viewport = CategoryViewportRect();
        if (alive.expired() || !HasArea(viewport) || !viewport.Contains(x, y)) {
            return -1;
        }
        for (std::size_t i = 0; i < composition->rows.size(); ++i) {
            const auto rect = composition->rows[i]->GetBounds();
            if (HasArea(rect) && rect.Contains(x, y)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    sky::ui::UIRect PreferencesDialog::ButtonRect(int index) const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        return !alive.expired() && isOpen && index >= 0 && index < 4 && composition->buttonItems[index] != nullptr
                   ? composition->buttonItems[index]->GetBounds()
                   : sky::ui::UIRect{};
    }

    int PreferencesDialog::ButtonAt(float x, float y) const
    {
        const std::weak_ptr<void> alive = lifetime;
        const_cast<PreferencesDialog *>(this)->SynchronizeLayout();
        if (alive.expired() || !isOpen) {
            return -1;
        }
        for (int i = 0; i < 4; ++i) {
            if (composition->buttonItems[i] == nullptr) {
                continue;
            }
            const auto rect = composition->buttonItems[i]->GetBounds();
            if (HasArea(rect) && rect.Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    sky::ui::UIRect PreferencesDialog::PageRect() const
    {
        return PageViewportRect();
    }

    void PreferencesDialog::ArrangeChildren(const sky::ui::UIRect &)
    {
        SynchronizeLayout();
    }

    void PreferencesDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen) {
            return;
        }
        PaintBackdrop(context);
        skin.DrawPanel(context, composition->panel->GetBounds(), "", false);
        const auto &theme = skin.Theme();
        uc::Fill(context, composition->categoryScroll->GetBounds(), theme.colors.section);
        const auto footer = composition->footer->GetBounds();
        uc::Fill(context, footer, theme.colors.section);
        uc::HLine(context, footer.left, footer.right, footer.top, theme.colors.border);
        const auto viewport = composition->categoryScroll->GetLayout().ContentRect(composition->categoryScroll->GetBounds());
        auto       track    = viewport;
        track.left          = viewport.right;
        track.right         = composition->categoryScroll->GetBounds().right;
        skin.DrawScrollbar(context, track, composition->categoryScroll->GetContentHeight(), composition->categoryScroll->GetScrollY());
    }

    sky::ui::UIEventResult PreferencesDialog::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        const auto                activeSession = session; // survive synchronous history/model observers
        const std::weak_ptr<void> alive         = lifetime;
        SynchronizeLayout();
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (formCaptured && event.pointerId != formPointerId) {
            return sky::ui::UIEventResult::HANDLED;
        }
        const bool ownsPointer = formCaptured && event.pointerId == formPointerId;
        // Popups may extend outside the form; a drag also retains MOVE/UP
        // outside its viewport. Do not touch members after forwarding user code.
        if (form->WantsPointerCapture() || (ownsPointer && event.action != sky::ui::UIPointerAction::DOWN)) {
            if (event.action == sky::ui::UIPointerAction::DOWN) {
                formCaptured  = true;
                formPointerId = event.pointerId;
            }
            if (event.action == sky::ui::UIPointerAction::UP && ownsPointer) {
                formCaptured = false;
            }
            return form->OnPointerEvent(event);
        }
        if (event.action == sky::ui::UIPointerAction::WHEEL) {
            const auto viewport = composition->categoryScroll->GetBounds();
            if (HasArea(viewport) && viewport.Contains(event.x, event.y)) {
                auto mapped = event;
                // Match editor wheel direction; ScrollView consumes offsets,
                // whereas the form consumes upward-positive wheel deltas.
                mapped.wheelDelta = -event.wheelDelta * skin.Theme().scale;
                composition->categoryScroll->OnPointerEvent(mapped);
                SynchronizeLayout(); // clamp and arrange before the next hit query
                return sky::ui::UIEventResult::HANDLED;
            }
            if (HasArea(form->GetBounds()) && form->GetBounds().Contains(event.x, event.y)) {
                return form->OnPointerEvent(event);
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            const int category = CategoryAt(event.x, event.y);
            if (alive.expired()) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const int button = ButtonAt(event.x, event.y);
            if (alive.expired()) {
                return sky::ui::UIEventResult::HANDLED;
            }
            hoverCategory = category;
            hoverButton   = button;
            UpdatePresentation();
            MarkPaintDirty();
            if (HasArea(form->GetBounds()) && form->GetBounds().Contains(event.x, event.y)) {
                return form->OnPointerEvent(event);
            }
            form->OnPointerLeave(event);
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::HANDLED;
        }
        const int button = ButtonAt(event.x, event.y);
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (button >= 0) {
            if (button == 0) {
                Commit(true);
            } else if (button == 1) {
                Close();
            } else if (button == 2) {
                Commit(false);
            } else {
                ResetPage();
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        const int category = CategoryAt(event.x, event.y);
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (category >= 0) {
            SetPageIndex(category);
            return sky::ui::UIEventResult::HANDLED;
        }
        if (HasArea(form->GetBounds()) && form->GetBounds().Contains(event.x, event.y)) {
            formCaptured  = true;
            formPointerId = event.pointerId;
            return form->OnPointerEvent(event);
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    void PreferencesDialog::OnPointerCancel(uint32_t pointerId)
    {
        if (formCaptured && pointerId != formPointerId) {
            return;
        }
        formCaptured = false;
        // Reset the widget before any caller can reattach this dialog. This
        // callback can destroy the owner, so no member access follows it.
        if (form != nullptr) {
            form->OnPointerCancel(pointerId);
        }
    }

    sky::ui::UIEventResult PreferencesDialog::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        const auto                activeSession = session; // keep history alive through synchronous observers
        const std::weak_ptr<void> alive         = lifetime;
        SynchronizeLayout();
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (!isOpen || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (form != nullptr && form->OnKeyEvent(event) == sky::ui::UIEventResult::HANDLED) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        constexpr uint32_t kEscape = 0x1B;
        if (event.keyCode == kEscape) {
            Close();
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::UNHANDLED;
    }

    sky::ui::UIEventResult PreferencesDialog::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        const auto                activeSession = session; // keep history alive through synchronous observers
        const std::weak_ptr<void> alive         = lifetime;
        SynchronizeLayout();
        if (alive.expired()) {
            return sky::ui::UIEventResult::HANDLED;
        }
        if (!isOpen || form == nullptr) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        return form->OnTextInput(event);
    }

} // namespace sky::editor
