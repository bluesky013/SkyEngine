//
// Reusable, resizable asset viewer widget (see asset-viewer-widget).
//

#include <editor/shell/AssetViewerWidget.h>

#include <core/platform/Platform.h>
#include <framework/asset/AssetDataBase.h>

#if defined(SKY_PLATFORM_WINDOWS)
// Declared manually to avoid <windows.h> macro pollution (e.g. RGB) in this TU.
extern "C" __declspec(dllimport) void *__stdcall ShellExecuteW(
    void *hwnd, const wchar_t *op, const wchar_t *file, const wchar_t *params, const wchar_t *dir, int show);
    #pragma comment(lib, "shell32.lib")
#endif

#include <editor/core/EditorCore.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/shell/AssetViewFlags.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/MenuBar.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/ToolBar.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>

#include <algorithm>
#include <limits>
#include <memory>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        constexpr uint32_t kTextSize    = 12;
        constexpr float    kRowHeight   = 20.0f;
        constexpr float    kPad         = 10.0f;
        constexpr float    kMinW        = 480.0f;
        constexpr float    kMinH        = 320.0f;
        constexpr float    kGrip        = 16.0f;
        constexpr float    kPanelGap    = 8.0f;
        constexpr float    kScrollbarW  = 8.0f;
        constexpr float    kTargetFormH = 172.0f; // fallback height when a form has no preferred height
        constexpr float    kCardMaxH    = 160.0f; // cap per-target card so long settings scroll inside the card

        // The viewer assigns each scrollable child's final rectangle after
        // measuring its column. This element supplies only the shared clip.
        class AssetContentViewport final : public sky::ui::UIElement {
        protected:
            void ArrangeChildren(const sky::ui::UIRect &) override
            {
            }
        };

    } // namespace

    AssetViewerWidget::AssetViewerWidget(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetLayoutVolatile(false);
        SetFocusable(true);
        SetVisible(false);
        // Clipping viewport for the scrollable content (viewer child; clipped to the content column).
        contentViewport = AddChild(std::make_unique<AssetContentViewport>());
        contentViewport->SetLayoutVolatile(false);
        contentViewport->SetClipsChildren(true);

        // Toolbar + body docked with DockPanel (no hand offsets); heights come from the theme
        // metrics so they DPI-scale. The menu bar is a DIRECT child added LAST so its open popup
        // paints above the toolbar/body (same reason the main shell adds the menu bar after the
        // toolbar). Items are filled from the active-scope actions when the viewer opens.
        const UiMetrics &m = GetDefaultUiTheme().metrics;
        topDock            = static_cast<sky::ui::DockPanel *>(AddChild(std::make_unique<sky::ui::DockPanel>()));
        toolBar = static_cast<ToolBar *>(topDock->Add(std::make_unique<ToolBar>(text), sky::ui::DockSlot{sky::ui::DockSide::Top, m.toolbarHeight}));
        toolBar->SetLayoutVolatile(false);
        bodyArea =
            static_cast<sky::ui::UIElement *>(topDock->Add(std::make_unique<sky::ui::UIElement>(), sky::ui::DockSlot{sky::ui::DockSide::Fill}));
        bodyArea->SetLayoutVolatile(false);
        menuBar = static_cast<MenuBar *>(AddChild(std::make_unique<MenuBar>(std::vector<MenuBar::Menu>{}, text)));
        menuBar->SetBarHeight(m.menuBarHeight);
        menuBar->SetLayoutVolatile(false);
        commandChangedId = assetCommands.AddChangeCallback([this]() {
            pendingSettingsReload = true;
            MarkLayoutDirty();
        });
    }

    AssetViewerWidget::~AssetViewerWidget()
    {
        actionLifetime.reset();
        // UIElement owns the forms, but these members own their models. Destroy
        // child forms now, while the models and borrowed services still exist.
        CancelFormInteractions();
        assetCommands.RemoveChangeCallback(commandChangedId);
        contentElements.clear();
        capturedElement = nullptr;
        ClearChildren();
    }

    void AssetViewerWidget::SetCatalog(EditorAssetCatalog *value)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (HasEditingForms()) {
            pendingCatalog    = value;
            hasPendingCatalog = true;
            MarkLayoutDirty();
            return;
        }
        hasPendingCatalog = false;
        pendingCatalog    = nullptr;
        if (catalog == value) {
            return;
        }
        CancelFormInteractions();
        assetCommands.Clear();
        catalog = value;
        if (IsOpen()) {
            Open(asset);
            if (alive.expired())
                return;
        }
        MarkLayoutDirty();
    }

    bool AssetViewerWidget::CanUndo() const
    {
        return IsOpen() && assetCommands.CanUndo();
    }

    bool AssetViewerWidget::CanRedo() const
    {
        return IsOpen() && assetCommands.CanRedo();
    }

    bool AssetViewerWidget::Undo()
    {
        if (!IsOpen()) {
            return false;
        }
        const bool result = assetCommands.Undo();
        if (pendingSettingsReload) {
            Reload();
        }
        return result;
    }

    bool AssetViewerWidget::Redo()
    {
        if (!IsOpen()) {
            return false;
        }
        const bool result = assetCommands.Redo();
        if (pendingSettingsReload) {
            Reload();
        }
        return result;
    }

    bool AssetViewerWidget::HasEditingForms() const
    {
        if (dataView != nullptr && dataView->Form().IsEditing()) {
            return true;
        }
        for (const auto &panel : panels) {
            if (panel.view != nullptr && panel.view->Form().IsEditing()) {
                return true;
            }
        }
        return false;
    }

    void AssetViewerWidget::CancelFormInteractions()
    {
        if (dataView != nullptr) {
            dataView->CancelInteractions();
        }
        for (auto &panel : panels) {
            if (panel.view != nullptr) {
                panel.view->CancelInteractions();
            }
        }
    }

    void AssetViewerWidget::Close()
    {
        if (HasEditingForms()) {
            pendingSession = PendingSessionAction::Close;
            MarkLayoutDirty();
            return;
        }
        ++actionSessionGeneration;
        pendingSession = PendingSessionAction::None;
        CancelFormInteractions();
        assetCommands.Clear();
        pendingSettingsReload = false;
        ModalDialog::Close();
    }

    void AssetViewerWidget::SynchronizeLayout()
    {
        const std::weak_ptr<void> alive = actionLifetime;
        // Domain observers may request a new asset/target set while a widget is
        // still committing. Drain only at the next safe host layout boundary,
        // never from the edit scope's destructor or its widget event tail.
        if (HasEditingForms()) {
            pendingSettingsReload = true;
            MarkLayoutDirty();
            return;
        }
        const auto requestedSession = pendingSession;
        const auto requestedAsset   = pendingAsset;
        pendingSession              = PendingSessionAction::None;
        if (hasPendingCatalog) {
            auto *nextCatalog = pendingCatalog;
            hasPendingCatalog = false;
            SetCatalog(nextCatalog);
            if (alive.expired())
                return;
        }
        if (requestedSession == PendingSessionAction::Open) {
            Open(requestedAsset);
            if (alive.expired())
                return;
        } else if (requestedSession == PendingSessionAction::Close) {
            Close();
            if (alive.expired())
                return;
        }
        if (pendingSettingsReload || (catalog != nullptr && catalogRevision != catalog->GetStateRevision())) {
            Reload();
        }
        if (SynchronizeDefaultUiTheme(*this, themeRevision)) {
            const auto &metrics = skin.Theme().metrics;
            topDock->SetSlot(toolBar, {sky::ui::DockSide::Top, metrics.toolbarHeight});
            menuBar->SetBarHeight(metrics.menuBarHeight);
        }
        if (dataView != nullptr) {
            dataView->SynchronizeLayout();
        }
        for (auto &panel : panels) {
            panel.view->SynchronizeLayout();
        }
        RefreshActions();
    }

    bool AssetViewerWidget::RefreshActions(bool force)
    {
        // UIElement::Arrange owns its dispatch stack. Defer user state queries
        // until the explicit host synchronization/input boundary outside it.
        if (arrangingContent) {
            if (force)
                actionChromeScope.clear();
            return true;
        }
        if (refreshingActions)
            return true;
        const std::weak_ptr<void> alive   = actionLifetime;
        const uint64_t            session = actionSessionGeneration;
        const auto                current = [this, alive, session]() { return !alive.expired() && session == actionSessionGeneration; };
        refreshingActions                 = true;
        struct RefreshGuard {
            std::weak_ptr<void> alive;
            bool               *busy;
            ~RefreshGuard()
            {
                if (!alive.expired())
                    *busy = false;
            }
        } guard{alive, &refreshingActions};
        auto      *registry = EditorActionRegistry::Get();
        const auto revision = registry->GetRevision();
        const auto scope    = EditorScopeStack::Get()->Active();
        if (force || revision != actionChromeRevision || scope != actionChromeScope) {
            const auto invoke = [this, alive](const std::string &id) {
                if (!alive.expired()) {
                    const auto handler = actionHandler;
                    if (handler)
                        handler(id);
                }
            };
            auto menus = ActionBarBuilder::BuildMenus(invoke);
            if (!current())
                return false;
            auto items = ActionBarBuilder::BuildToolBarItems(invoke);
            if (!current())
                return false;
            menuBar->SetMenus(std::move(menus), true);
            if (!current())
                return false;
            toolBar->SetItems(std::move(items));
            actionChromeRevision = revision;
            actionChromeScope    = scope;
        }
        if (!menuBar->RefreshActionStates() || !current())
            return false;
        if (!toolBar->RefreshActionStates() || !current())
            return false;
        return true;
    }

    void AssetViewerWidget::Open(const Uuid &uuid)
    {
        if (HasEditingForms()) {
            pendingAsset   = uuid;
            pendingSession = PendingSessionAction::Open;
            MarkLayoutDirty();
            return;
        }
        ++actionSessionGeneration;
        pendingSession = PendingSessionAction::None;
        CancelFormInteractions();
        assetCommands.Clear();
        asset = uuid;
        assetName.clear();
        assetType.clear();
        assetPath.clear();
        viewFlags = AssetViewFlags::BUNDLE;

        if (catalog != nullptr) {
            EditorAssetItem item;
            if (catalog->Find(uuid, item)) {
                assetName = item.name;
                assetType = item.type;
                assetPath = item.path;
            }
        }

        std::string ext;
        if (const auto dot = assetPath.find_last_of('.'); dot != std::string::npos) {
            ext = assetPath.substr(dot);
        }
        viewFlags = DeriveAssetViewFlags(assetType, ext);

        contentScroll = 0.0f;
        RebuildCustomContent();
        RebuildDataView();
        RebuildTargetPanels();
        if (!RefreshActions(true))
            return;
        Reload();
        ModalDialog::Open();
        MarkLayoutDirty();
    }

    void AssetViewerWidget::RebuildCustomContent()
    {
        contentElements.clear();
        capturedElement = nullptr;
        if (customContent != nullptr) {
            contentViewport->RemoveChild(customContent);
            customContent = nullptr;
        }

        if (!HasAssetViewFlag(viewFlags, AssetViewFlags::DATA)) {
            return;
        }

        IAssetViewProvider *provider = AssetViewProviderRegistry::Get()->Find(assetType);
        if (provider == nullptr) {
            return;
        }
        if (auto content = provider->CreateContent(asset, assetType, textSystem)) {
            customContent = contentViewport->AddChild(std::move(content));
            AddContentElement(customContent);
        }
    }

    void AssetViewerWidget::RebuildDataView()
    {
        if (dataView != nullptr) {
            contentViewport->RemoveChild(dataView);
            dataView = nullptr;
        }
        registeredViewData = AssetViewData{};

        // A custom provider content widget takes precedence over the generic reflected view.
        if (!HasAssetViewFlag(viewFlags, AssetViewFlags::DATA) || customContent != nullptr) {
            return;
        }

        auto data = AssetViewDataRegistry::Get()->Build(assetType, asset);
        if (!data.IsValid()) {
            return;
        }
        registeredViewData = std::move(data);

        auto view = std::make_unique<ReflectedFormView>(
            textSystem, assetName,
            ReflectedFormContext{assetCommands, EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()});
        view->SetLayoutVolatile(false);
        view->SetHeaderVisible(false);
        view->Bind(PropertyObject{registeredViewData.object.Data(), registeredViewData.type});
        dataView = static_cast<ReflectedFormView *>(contentViewport->AddChild(std::move(view)));
        AddContentElement(dataView);
    }

    void AssetViewerWidget::RebuildTargetPanels()
    {
        for (auto &panel : panels) {
            if (panel.view != nullptr) {
                contentViewport->RemoveChild(panel.view);
            }
        }
        panels.clear();

        // BUNDLE and DATA render together; only skip when there is no cook-settings section.
        if (catalog == nullptr || !HasAssetViewFlag(viewFlags, AssetViewFlags::BUNDLE)) {
            return;
        }

        const auto targets = catalog->GetTargetInfos(asset);
        for (const auto &info : targets) {
            auto view = std::make_unique<ReflectedFormView>(
                textSystem, info.target,
                ReflectedFormContext{assetCommands, EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(), GetDefaultUiTheme()});
            view->SetLayoutVolatile(false);
            view->SetHeaderVisible(false);
            const std::string   target        = info.target;
            const Uuid          editedAsset   = asset;
            EditorAssetCatalog *editedCatalog = catalog;
            view->Form().SetEditCommandFactory([editedCatalog, editedAsset, target](const PropertyDescriptor &property, Any value) {
                return editedCatalog->MakeCookSettingsPropertyEditCommand(editedAsset, target, property.GetName(), std::move(value));
            });
            view->SetOnEdited([this]() { Reload(); });
            TargetPanel panel;
            panel.target = target;
            panel.view   = static_cast<ReflectedFormView *>(contentViewport->AddChild(std::move(view)));
            AddContentElement(panel.view);
            panels.push_back(std::move(panel));
        }
    }

    void AssetViewerWidget::Reload()
    {
        if (HasEditingForms()) {
            pendingSettingsReload = true;
            MarkLayoutDirty();
            return;
        }
        pendingSettingsReload = false;
        if (catalog == nullptr) {
            return;
        }
        const auto targets =
            HasAssetViewFlag(viewFlags, AssetViewFlags::BUNDLE) ? catalog->GetTargetInfos(asset) : std::vector<EditorAssetTargetInfo>{};
        bool targetsChanged = targets.size() != panels.size();
        for (size_t i = 0; !targetsChanged && i < targets.size(); ++i) {
            targetsChanged = targets[i].target != panels[i].target;
        }
        bool invalidateHistory = targetsChanged;
        if (targetsChanged) {
            CancelFormInteractions();
            contentElements.clear();
            capturedElement = nullptr;
            if (customContent != nullptr) {
                AddContentElement(customContent);
            }
            if (dataView != nullptr) {
                AddContentElement(dataView);
            }
            RebuildTargetPanels();
        }
        for (auto &panel : panels) {
            // Cancel while the old Any is still live, then detach before its
            // heap-backed storage is released by assignment.
            panel.view->CancelInteractions();
            panel.view->Unbind();
            const Uuid oldSchema =
                panel.settings.IsValid() && panel.settings.object.Info() != nullptr ? panel.settings.object.Info()->registeredId : Uuid{};
            panel.settings = catalog->GetCookSettings(asset, panel.target);
            if (panel.settings.IsValid()) {
                if (oldSchema && !(oldSchema == panel.settings.object.Info()->registeredId)) {
                    invalidateHistory = true;
                }
                const PropertyObject object{panel.settings.object.Data(), panel.settings.type};
                const PropertyObject baseline{panel.settings.baseline.Data(), panel.settings.type};
                panel.view->Bind(object, &baseline);
                panel.view->SetVisible(true);
            } else {
                invalidateHistory = true;
                panel.view->SetVisible(false);
            }
        }
        std::string currentName;
        if (!catalog->GetName(asset, currentName)) {
            invalidateHistory = true;
            if (dataView != nullptr) {
                dataView->Unbind();
                dataView->SetVisible(false);
            }
        }
        if (invalidateHistory && (assetCommands.CanUndo() || assetCommands.CanRedo())) {
            assetCommands.Clear();
        }
        pendingSettingsReload = false;
        catalogRevision       = catalog->GetStateRevision();
        MarkLayoutDirty();
    }

    sky::ui::UIRect AssetViewerWidget::PanelRect() const
    {
        return CenteredPanel(panelW, panelH, kMinW, kMinH);
    }

    sky::ui::UIRect AssetViewerWidget::ContentRect(const sky::ui::UIRect &panel) const
    {
        const float header = skin.Theme().metrics.panelHeaderHeight;
        return {panel.left, panel.top + header, panel.right, panel.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::RightRect(const sky::ui::UIRect &content) const
    {
        const float previewW = (content.right - content.left) * 0.38f;
        return {content.left + previewW + kPad, content.top + kPad, content.right - kPad, content.bottom - kPad};
    }

    sky::ui::UIRect AssetViewerWidget::CloseRect(const sky::ui::UIRect &panel) const
    {
        return {panel.right - 26.0f, panel.top + 6.0f, panel.right - 6.0f, panel.top + 26.0f};
    }

    void AssetViewerWidget::SaveAsset()
    {
        const std::weak_ptr<void> alive   = actionLifetime;
        const uint64_t            session = actionSessionGeneration;
        CancelFormInteractions();
        if (alive.expired() || session != actionSessionGeneration)
            return;
        SynchronizeLayout();
        if (alive.expired() || session != actionSessionGeneration)
            return;
        if (IAssetViewProvider *provider = AssetViewProviderRegistry::Get()->Find(assetType)) {
            provider->SaveContent(asset, assetType, customContent);
        }
        if (catalog != nullptr) {
            AssetCookService::Get()->RequestCook(asset);
        }
        MarkPaintDirty();
    }

    void AssetViewerWidget::OpenAsset()
    {
        // "Open" = open the asset's source file with the OS default handler.
        std::string sourcePath;
        auto       *db = AssetDataBase::Get();
        if (db == nullptr || !db->GetAbsoluteSourcePath(asset, sourcePath) || sourcePath.empty()) {
            return;
        }
#if defined(SKY_PLATFORM_WINDOWS)
        const std::wstring wide(sourcePath.begin(), sourcePath.end());
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, 1 /* SW_SHOWNORMAL */);
#endif
    }

    sky::ui::UIRect AssetViewerWidget::ResizeRect(const sky::ui::UIRect &panel) const
    {
        return {panel.right - kGrip, panel.bottom - kGrip, panel.right, panel.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::ScrollTrackRect() const
    {
        const sky::ui::UIRect right   = RightRect(ContentRect(PanelRect()));
        const float           viewTop = right.top + 2.0f * kRowHeight + 4.0f;
        return {right.right - kScrollbarW, viewTop, right.right, right.bottom};
    }

    sky::ui::UIRect AssetViewerWidget::ScrollThumbRect() const
    {
        const sky::ui::UIRect track     = ScrollTrackRect();
        const float           viewH     = std::max(0.0f, track.bottom - track.top);
        const float           maxScroll = std::max(0.0f, contentTotal - contentViewH);
        const float           span      = contentTotal > 0.0f ? std::min(1.0f, contentViewH / contentTotal) : 1.0f;
        const float           thumbH    = std::max(24.0f, viewH * span);
        const float           t         = maxScroll > 0.0f ? contentScroll / maxScroll : 0.0f;
        const float           thumbTop  = track.top + (viewH - thumbH) * t;
        return {track.left + 1.0f, thumbTop, track.right - 1.0f, thumbTop + thumbH};
    }

    void AssetViewerWidget::ArrangeChildren(const sky::ui::UIRect & /*content*/)
    {
        if (!IsOpen()) {
            return;
        }
        const std::weak_ptr<void> alive = actionLifetime;
        arrangingContent                = true;
        SynchronizeLayout();
        if (alive.expired())
            return;
        arrangingContent = false;
        if (!IsOpen())
            return;
        const sky::ui::UIRect content = skin.PanelContentRect(PanelRect());

        // Menu bar (top, a direct child added last so its popup paints above the toolbar) and the
        // toolbar+body dock below it.
        const float menuH = skin.Theme().metrics.menuBarHeight;
        menuBar->Arrange({content.left, content.top, content.right, content.top + menuH});
        topDock->Arrange({content.left, content.top + menuH, content.right, content.bottom});
        const sky::ui::UIRect right   = RightRect(bodyArea->GetBounds());
        const float           viewTop = right.top + 2.0f * kRowHeight + 4.0f;
        const float           viewH   = std::max(0.0f, right.bottom - viewTop);
        contentViewH                  = viewH;

        // Right region scrolls as one: the DATA section (asset view) then one form per cook target.
        const bool hasData = (customContent != nullptr) || (dataView != nullptr);

        float formsTotal = 0.0f;
        for (auto &tp : panels) {
            const float formH = std::min((tp.view != nullptr) ? tp.view->GetPreferredHeight() : kTargetFormH, kCardMaxH);
            tp.h              = kRowHeight + formH + kPanelGap;
            formsTotal += tp.h;
        }

        const auto measureData = [&](float width) {
            if (dataView != nullptr) {
                return dataView->GetPreferredHeight();
            }
            if (customContent != nullptr) {
                const auto desired = customContent->MeasureDesired({{}, {std::max(0.0f, width), std::numeric_limits<float>::infinity()}});
                return std::max(desired.height, kRowHeight);
            }
            return 0.0f;
        };
        float                 dataH  = measureData(right.Width());
        const bool            hasBar = formsTotal + (hasData ? dataH + kPanelGap : 0.0f) > viewH + 0.5f;
        const float           barW   = hasBar ? kScrollbarW : 0.0f;
        const sky::ui::UIRect contentRight{right.left, right.top, std::max(right.left, right.right - barW), right.bottom};
        if (hasBar && customContent != nullptr) {
            // Reserve a stable scrollbar column, then measure at the exact
            // width that will be arranged (including any extra wrapped lines).
            dataH = measureData(contentRight.Width());
        }
        contentTotal          = formsTotal + (hasData ? dataH + kPanelGap : 0.0f);
        const float maxScroll = std::max(0.0f, contentTotal - viewH);
        contentScroll         = std::clamp(contentScroll, 0.0f, maxScroll);

        float py = viewTop - contentScroll;

        if (contentViewport != nullptr) {
            contentViewport->Arrange(right);
        }

        sky::ui::UIElement *dataElement = customContent != nullptr ? customContent : static_cast<sky::ui::UIElement *>(dataView);
        if (hasData && dataElement != nullptr) {
            // Full height positioned at py (not clamped to the viewport); the parent clip scrolls it.
            const float top     = py;
            const float bottom  = py + dataH;
            const bool  visible = (bottom > viewTop) && (top < right.bottom);
            // Carry the same logical budget used for measurement instead of
            // reconstructing it by subtracting absolute-coordinate edges.
            dataElement->ArrangeIfNeeded({contentRight.left, top, contentRight.right, bottom}, {contentRight.Width(), dataH});
            dataElement->SetVisible(visible);
            py += dataH + kPanelGap;
        } else {
            if (customContent != nullptr) {
                customContent->SetVisible(false);
            }
            if (dataView != nullptr) {
                dataView->SetVisible(false);
            }
        }

        for (auto &tp : panels) {
            const float formTop    = py + kRowHeight;
            const float formBottom = py + kRowHeight + (tp.h - kRowHeight - kPanelGap);
            const bool  visible    = tp.settings.IsValid() && (formBottom > viewTop) && (formTop < right.bottom);
            tp.view->Arrange({contentRight.left, formTop, contentRight.right, formBottom});
            tp.view->SetVisible(visible);
            py += tp.h;
        }
    }

    void AssetViewerWidget::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!IsOpen()) {
            return;
        }
        PaintBackdrop(context);

        const sky::ui::UIRect panel = PanelRect();
        skin.DrawPanel(context, panel, assetName.empty() ? std::string("Asset Viewer") : assetName, true);
        const sky::ui::UIRect body     = bodyArea->GetBounds();
        const float           previewW = body.Width() * 0.38f;
        const sky::ui::UIRect preview{body.left + kPad, body.top + kPad, body.left + previewW, body.bottom - kPad};
        const sky::ui::UIRect right = RightRect(body);

        uc::RoundedField(context, preview, uc::color::Section, uc::color::BorderSoft, 4.0f);
        IAssetPreviewProvider *provider   = AssetPreviewProviderRegistry::Get()->GetProvider();
        const bool             hasPreview = provider != nullptr && provider->GetPreview(asset, assetType);
        uc::Text(context, hasPreview ? "preview (reserved)" : "no preview", kTextSize, preview, uc::color::TextDisabled, textSystem,
                 uc::HAlign::Center, uc::VAlign::Middle);

        uc::Text(context, assetName + "  [" + assetType + "]", kTextSize, {right.left, right.top, right.right, right.top + kRowHeight},
                 uc::color::Text, textSystem);
        uc::Text(context, assetPath, kTextSize, {right.left, right.top + kRowHeight, right.right, right.top + 2.0f * kRowHeight},
                 uc::color::TextMuted, textSystem);

        const float         viewTop      = right.top + 2.0f * kRowHeight + 4.0f;
        const bool          hasBar       = contentTotal > contentViewH + 0.5f;
        const float         contentRight = right.right - (hasBar ? kScrollbarW : 0.0f);
        float               py           = viewTop - contentScroll;
        sky::ui::UIElement *dataElement  = customContent != nullptr ? customContent : static_cast<sky::ui::UIElement *>(dataView);
        if (dataElement != nullptr) {
            py += dataElement->GetBounds().Height() + kPanelGap;
        }
        for (const auto &tp : panels) {
            const sky::ui::UIRect labelRect{right.left, py, contentRight, py + kRowHeight};
            if (labelRect.bottom > viewTop && labelRect.top < right.bottom) {
                skin.DrawSectionHeader(context, labelRect, tp.target);
            }
            py += tp.h;
        }

        // Outer scrollbar (hover + drag + wheel): one bar for the whole right region.
        if (hasBar) {
            const sky::ui::UIRect track = ScrollTrackRect();
            uc::RoundedField(context, track, uc::color::Section, uc::color::BorderSoft, kScrollbarW * 0.5f);
            const sky::ui::UIRect thumb      = ScrollThumbRect();
            const auto            thumbColor = (dragScroll || hoverScroll) ? uc::color::Text : uc::color::TextMuted;
            uc::RoundedField(context, thumb, thumbColor, uc::color::BorderSoft, kScrollbarW * 0.5f);
        }

        const sky::ui::UIRect close = CloseRect(panel);
        uc::Text(context, "x", kTextSize, close, hoverClose ? uc::color::Text : uc::color::TextMuted, textSystem, uc::HAlign::Center,
                 uc::VAlign::Middle, true);

        const sky::ui::UIRect grip      = ResizeRect(panel);
        const auto            gripColor = (hoverResize || resizing) ? uc::color::Text : uc::color::TextDisabled;
        uc::Text(context, "//", kTextSize, grip, gripColor, textSystem, uc::HAlign::Center, uc::VAlign::Middle, true);
    }

    sky::ui::UIEventResult AssetViewerWidget::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (!IsOpen()) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (!RefreshActions())
            return sky::ui::UIEventResult::UNHANDLED;
        const sky::ui::UIRect panel = PanelRect();

        if (menuBar != nullptr) {
            const sky::ui::UIEventResult menuResult = menuBar->OnPointerEvent(event);
            if (alive.expired())
                return sky::ui::UIEventResult::HANDLED;
            if (menuResult == sky::ui::UIEventResult::HANDLED) {
                MarkPaintDirty();
                return menuResult;
            }
        }

        if (toolBar != nullptr) {
            const sky::ui::UIEventResult toolbarResult = toolBar->OnPointerEvent(event);
            if (alive.expired())
                return sky::ui::UIEventResult::HANDLED;
            if (toolbarResult == sky::ui::UIEventResult::HANDLED) {
                MarkPaintDirty();
                return toolbarResult;
            }
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (resizing) {
                panelW = std::clamp(resizeStartW + (event.x - resizeStartX), kMinW, GetBounds().right - 2.0f * kPad);
                panelH = std::clamp(resizeStartH + (event.y - resizeStartY), kMinH, GetBounds().bottom - 2.0f * kPad);
                MarkArrangeDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (dragScroll) {
                const float viewH     = std::max(1.0f, contentViewH);
                const float maxScroll = std::max(0.0f, contentTotal - contentViewH);
                contentScroll         = std::clamp(scrollDragStartScroll + (event.y - scrollDragStartY) * (contentTotal / viewH), 0.0f, maxScroll);
                MarkArrangeDirty();
                return sky::ui::UIEventResult::HANDLED;
            }
            hoverClose  = CloseRect(panel).Contains(event.x, event.y);
            hoverResize = ResizeRect(panel).Contains(event.x, event.y);
            hoverScroll = contentTotal > contentViewH && ScrollTrackRect().Contains(event.x, event.y);
            MarkPaintDirty();
        } else if (event.action == sky::ui::UIPointerAction::DOWN) {
            if (CloseRect(panel).Contains(event.x, event.y)) {
                Close();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (ResizeRect(panel).Contains(event.x, event.y)) {
                resizing     = true;
                resizeStartX = event.x;
                resizeStartY = event.y;
                resizeStartW = panelW;
                resizeStartH = panelH;
                return sky::ui::UIEventResult::HANDLED;
            }
            if (contentTotal > contentViewH && ScrollTrackRect().Contains(event.x, event.y)) {
                dragScroll            = true;
                scrollDragStartY      = event.y;
                scrollDragStartScroll = contentScroll;
                return sky::ui::UIEventResult::HANDLED;
            }
        } else if (event.action == sky::ui::UIPointerAction::UP) {
            resizing   = false;
            dragScroll = false;
        } else if (event.action == sky::ui::UIPointerAction::WHEEL) {
            // Scroll the hovered child first (nested scrolling); fall back to the outer list.
            if (RouteContentPointer(event) == sky::ui::UIEventResult::HANDLED) {
                return sky::ui::UIEventResult::HANDLED;
            }
            const sky::ui::UIRect right = RightRect(ContentRect(panel));
            if (right.Contains(event.x, event.y) && contentTotal > contentViewH) {
                contentScroll = std::clamp(contentScroll - event.wheelDelta * 0.6f, 0.0f, contentTotal - contentViewH);
                MarkArrangeDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        // Forward to content children (hover, drag capture, overlay priority).
        RouteContentPointer(event);
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult AssetViewerWidget::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        // Shortcuts (Ctrl+S/O/Z/Y, Esc) are resolved by EditorShell from the scoped actions'
        // accelerators before this is reached; only content editing keys land here.
        return RouteContentKey(event);
    }

    sky::ui::UIEventResult AssetViewerWidget::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        return RouteContentText(event);
    }

} // namespace sky::editor
