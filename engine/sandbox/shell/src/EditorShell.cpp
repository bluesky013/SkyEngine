//
// Created on 2026/09/22.
//

#include <editor/core/EditorCore.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/extension/IconRegistry.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/layout/DockInteraction.h>
#include <editor/core/preferences/BuiltinPreferencePages.h>
#include <editor/core/shell/ShellModels.h>
#include <editor/shell/AssetViewerWidget.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/FileBrowserDialog.h>
#include <editor/shell/NewWorldDialog.h>
#include <editor/shell/PanelView.h>
#include <editor/shell/PreferencesDialog.h>

#include <editor/shell/AssetViewFlags.h>
#include <editor/shell/PresetEditorPanel.h>
#include <editor/shell/ReflectedInspectorPanel.h>
#include <editor/shell/ReflectionDemoPanel.h>
#include <editor/shell/WorldConfigPanel.h>
#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/UiSkin.h>

#include <editor/shell/panels/AssetBrowserPanel.h>
#include <editor/shell/panels/ServicePanels.h>
#include <editor/shell/widgets/DockWidgets.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/MenuBar.h>
#include <editor/widgets/StatusBar.h>
#include <editor/widgets/ToolBar.h>
#include <framework/project/ExportPreset.h>

#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cmath>
#include <core/logger/Logger.h>
#include <cstdio>
#include <functional>

static const char *TAG = "EditorShell";

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        void SynchronizePanelLayout(sky::ui::UIElement *element)
        {
            if (auto *panel = dynamic_cast<WorldConfigPanel *>(element)) {
                panel->SynchronizeLayout();
            } else if (auto *viewer = dynamic_cast<AssetViewerWidget *>(element)) {
                viewer->SynchronizeLayout();
            } else if (auto *form = dynamic_cast<ReflectedFormView *>(element)) {
                form->SynchronizeLayout();
            }
        }

        // Child rectangles of a split, via the shared core geometry helper.
        std::vector<sky::ui::UIRect> SplitRect(LayoutNode *node, const sky::ui::UIRect &rect)
        {
            std::vector<LayoutRect> children;
            ComputeChildRects(*static_cast<SplitNode *>(node), LayoutRect{rect.left, rect.top, rect.right, rect.bottom}, children);
            std::vector<sky::ui::UIRect> out;
            out.reserve(children.size());
            for (const LayoutRect &child : children) {
                out.push_back(sky::ui::UIRect{child.left, child.top, child.right, child.bottom});
            }
            return out;
        }
    } // namespace

    EditorShell::EditorShell() : context(std::make_unique<sky::ui::UIContext>())
    {
        eventRouter = std::make_unique<sky::ui::UIEventRouter>(*context);
        ApplyTheme();
    }

    EditorShell::~EditorShell()
    {
        actionLifetime.reset();
    }

    void EditorShell::ApplyTheme(sky::ui::UIContext *target)
    {
        sky::ui::UITheme &theme = (target != nullptr ? target : context.get())->GetTheme();

        const UiTheme &ui = GetDefaultUiTheme();

        sky::ui::UIStyle panel;
        panel.SetBackgroundColor(ui.colors.panel);
        panel.SetBorderColor(ui.colors.borderSoft);
        panel.SetBorderWidth(1.0f);
        theme.SetStyle("panel", panel);

        sky::ui::UIStyle title;
        title.SetTextColor(ui.colors.text);
        theme.SetStyle("panel-title", title);

        sky::ui::UIStyle bar;
        bar.SetBackgroundColor(ui.colors.toolbar);
        bar.SetTextColor(ui.colors.text);
        theme.SetStyle("menu-bar", bar);

        sky::ui::UIStyle item;
        item.SetButtonColors(0x00000000, ui.colors.rowHover, ui.colors.toolbar);
        item.SetTextColor(ui.colors.text);
        theme.SetStyle("menu-item", item);
    }

    void EditorShell::SetTextSystem(sky::ui::UITextSystem *text)
    {
        textSystem = text;
    }
    void EditorShell::SetLayout(LayoutModel *layout)
    {
        layoutModel = layout;
    }
    void EditorShell::SetPanelRegistry(PanelRegistry *registry)
    {
        panelRegistry = registry;
    }
    void EditorShell::SetSelection(SelectionService *value)
    {
        selection = value;
    }
    void EditorShell::SetLogService(LogService *value)
    {
        logService = value;
    }
    void EditorShell::SetCommandController(CommandController *value)
    {
        commandController = value;
    }
    void EditorShell::SetInspectorModel(PropertyModel *model)
    {
        inspectorModel = model;
    }

    void EditorShell::SetPropertySource(IEditorPropertySource *source)
    {
        propertySource = source;
    }

    void EditorShell::SetAssetCatalog(EditorAssetCatalog *catalog)
    {
        assetCatalog = catalog;
    }

    void EditorShell::SetAssetActionHandler(std::function<bool(const std::string &)> handler)
    {
        assetActionHandler = std::move(handler);
    }

    void EditorShell::SetAssetOpenHandler(std::function<void(const Uuid &)> handler)
    {
        assetOpenHandler = std::move(handler);
    }

    void EditorShell::SetAssetRenameHandler(std::function<void(const std::string &, const std::string &)> handler)
    {
        assetRenameHandler = std::move(handler);
    }

    void EditorShell::BeginAssetRename()
    {
        if (assetPanel != nullptr) {
            assetPanel->BeginRename();
        }
    }

    void EditorShell::SetUiScale(float scale)
    {
        deviceUiScale       = ComputeEditorUiScale(scale, 1.0f);
        const UiTheme theme = MakeDarkTheme(GetSurfaceUiScale());
        uiScale             = theme.scale;
        SetDefaultUiTheme(theme);
    }

    void EditorShell::SetUserUiScale(float scale)
    {
        userUiScale = std::isfinite(scale) ? std::clamp(scale, 0.5f, 2.0f) : 1.0f;
        uiScale     = GetSurfaceUiScale();
        SetDefaultUiTheme(MakeDarkTheme(uiScale));
    }

    float EditorShell::GetSurfaceUiScale(uint32_t surfaceId) const
    {
        float device = deviceUiScale;
        if (surfaceId != 0) {
            if (const auto it = surfaceScales.find(surfaceId); it != surfaceScales.end()) {
                device = it->second;
            }
        }
        return ComputeEditorUiScale(device, userUiScale);
    }

    void EditorShell::SetSurfaceScale(uint32_t surfaceId, float scale)
    {
        if (surfaceId == 0) {
            SetUiScale(scale);
            return;
        }
        surfaceScales[surfaceId] = ComputeEditorUiScale(scale, 1.0f);
    }

    void EditorShell::RegisterPanelView(const std::string &panelId, PanelViewFactory factory)
    {
        viewFactories[panelId] = std::move(factory);
    }

    void EditorShell::RegisterBuiltinPanelViews()
    {
        auto titleOf = [this](const char *id, const char *fallback) -> std::string {
            if (panelRegistry != nullptr) {
                if (const PanelInfo *info = panelRegistry->Find(id)) {
                    return info->title;
                }
            }
            return fallback;
        };

        RegisterPanelView("viewport",
                          [this, t = titleOf("viewport", "Viewport")]() { return std::unique_ptr<sky::ui::UIElement>(new ShellPanel(t)); });
        RegisterPanelView("outliner", [this, t = titleOf("outliner", "Outliner")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutlinerPanel(selection, textSystem, t, propertySource));
        });
        RegisterPanelView("inspector", [this, t = titleOf("inspector", "Inspector")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ReflectedInspectorPanel(textSystem, propertySource, selection, t));
        });
        RegisterPanelView("config", [this, t = titleOf("config", "Config")]() {
            auto panel = std::make_unique<WorldConfigPanel>(textSystem, WorldConfigPanel::DocumentProvider{}, t);
            panel->SetDocument(worldDocument);
            return panel;
        });
        RegisterPanelView("export-presets", [this, t = titleOf("export-presets", "Export Presets")]() {
            return std::unique_ptr<sky::ui::UIElement>(new PresetEditorPanel(
                textSystem, [this]() { return exportPresets; },
                [this]() {
                    if (exportPresets != nullptr && !exportPresetsPath.empty()) {
                        exportPresets->Save(exportPresetsPath);
                    }
                },
                t));
        });
        RegisterPanelView("refldemo", [this, t = titleOf("refldemo", "Reflection Demo")]() { return CreateReflectionDemoPanel(textSystem, t); });
        RegisterPanelView("outputlog", [this, t = titleOf("outputlog", "Output Log")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutputLogPanel(logService, textSystem, t));
        });
        RegisterPanelView("console", [this, t = titleOf("console", "Console")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ConsolePanel(commandController, textSystem, t));
        });
        RegisterPanelView("assets", [this, t = titleOf("assets", "Assets")]() {
            auto panel = std::make_unique<AssetBrowserPanel>(t);
            panel->SetTextSystem(textSystem);
            panel->SetCatalog(assetCatalog);
            panel->SetSelection(selection);
            panel->SetActionHandler(assetActionHandler);
            panel->SetOpenHandler(assetOpenHandler);
            panel->SetRenameHandler(assetRenameHandler);
            assetPanel = panel.get();
            return std::unique_ptr<sky::ui::UIElement>(std::move(panel));
        });
    }

    std::unique_ptr<sky::ui::UIElement> EditorShell::MakePanelView(const std::string &panelId)
    {
        std::string title = panelId;
        if (panelRegistry != nullptr) {
            if (const PanelInfo *info = panelRegistry->Find(panelId)) {
                title = info->title;
            }
        }
        std::unique_ptr<sky::ui::UIElement> element;
        const auto                          it = viewFactories.find(panelId);
        if (it != viewFactories.end() && it->second) {
            element = it->second();
        }
        if (element == nullptr) {
            element = std::make_unique<ShellPanel>(title);
        }
        element->AddStyleClass("panel");
        if (auto *frame = dynamic_cast<ShellPanel *>(element.get())) {
            frame->SetTextSystem(textSystem);
        }
        element->SetName(panelId);
        return element;
    }

    sky::ui::UIElement *EditorShell::AdoptView(const std::string &panelId, std::unique_ptr<sky::ui::UIElement> element)
    {
        if (element == nullptr) {
            return nullptr;
        }
        // Detached views may have missed theme changes or come from another
        // DPI surface. Their nested caches must follow the new owner, too.
        uint64_t adoptedThemeRevision = 0;
        SynchronizeDefaultUiTheme(*element, adoptedThemeRevision);
        sky::ui::UIElement *raw = context->AddChild(std::move(element));
        attachedViews[panelId]  = raw;
        return raw;
    }

    void EditorShell::HarvestViews()
    {
        for (auto &entry : attachedViews) {
            if (entry.second == nullptr) {
                continue;
            }
            if (auto owned = context->GetRoot()->RemoveChild(entry.second)) {
                detachedViews[entry.first] = std::move(owned);
            }
        }
        attachedViews.clear();
    }

    sky::ui::UIElement *EditorShell::CreatePanelView(const std::string &panelId)
    {
        if (const auto it = attachedViews.find(panelId); it != attachedViews.end()) {
            return it->second;
        }
        std::unique_ptr<sky::ui::UIElement> element;
        if (const auto it = detachedViews.find(panelId); it != detachedViews.end()) {
            element = std::move(it->second);
            detachedViews.erase(it);
        } else {
            element = MakePanelView(panelId);
        }
        return AdoptView(panelId, std::move(element));
    }

    sky::ui::UIElement *EditorShell::GetPanelView(const std::string &panelId)
    {
        if (const auto floating = panelSurfaceIds.find(panelId); floating != panelSurfaceIds.end()) {
            if (const auto surface = surfaces.find(floating->second); surface != surfaces.end())
                return surface->second.view;
        }
        if (const auto it = attachedViews.find(panelId); it != attachedViews.end()) {
            return it->second;
        }
        if (const auto it = detachedViews.find(panelId); it != detachedViews.end()) {
            return it->second.get();
        }
        std::unique_ptr<sky::ui::UIElement> element = MakePanelView(panelId);
        sky::ui::UIElement                 *raw     = element.get();
        detachedViews[panelId]                      = std::move(element);
        return raw;
    }

    std::unique_ptr<sky::ui::UIElement> EditorShell::TakePanelView(const std::string &panelId)
    {
        if (const auto it = attachedViews.find(panelId); it != attachedViews.end()) {
            sky::ui::UIElement *raw = it->second;
            attachedViews.erase(it);
            return context->GetRoot()->RemoveChild(raw);
        }
        if (const auto it = detachedViews.find(panelId); it != detachedViews.end()) {
            auto owned = std::move(it->second);
            detachedViews.erase(it);
            return owned;
        }
        return nullptr;
    }

    bool EditorShell::IsPanelAttached(const std::string &panelId) const
    {
        return attachedViews.find(panelId) != attachedViews.end();
    }

    bool EditorShell::ConsumeLayoutDirty()
    {
        const bool dirty = layoutDirty;
        layoutDirty      = false;
        return dirty;
    }

    uint32_t EditorShell::FloatPanelToSurface(const std::string &panelId, uint32_t surfaceId, const FloatingPanel &geometry)
    {
        if (layoutModel != nullptr) {
            layoutModel->FloatPanel(panelId, geometry);
            layoutDirty = true;
        }
        return CreateSurfaceContext(panelId, surfaceId);
    }

    uint32_t EditorShell::RestoreFloatingPanel(const std::string &panelId, uint32_t surfaceId)
    {
        // The panel is already in the model's floating set (restored layout).
        return CreateSurfaceContext(panelId, surfaceId);
    }

    uint32_t EditorShell::CreateSurfaceContext(const std::string &panelId, uint32_t surfaceId)
    {
        if (const auto it = panelSurfaceIds.find(panelId); it != panelSurfaceIds.end()) {
            return it->second;
        }
        Surface surface;
        surface.context             = std::make_unique<sky::ui::UIContext>();
        surface.router              = std::make_unique<sky::ui::UIEventRouter>(*surface.context);
        surface.context->GetTheme() = context->GetTheme();
        surface.panelId             = panelId;

        std::unique_ptr<sky::ui::UIElement> view = TakePanelView(panelId);
        if (view == nullptr) {
            view = MakePanelView(panelId);
        }
        surface.view = surface.context->AddChild(std::move(view));
        SynchronizeDefaultUiTheme(*surface.context->GetRoot(), surface.themeRevision);
        SynchronizePanelLayout(surface.view);
        surface.view->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});

        surfaces.emplace(surfaceId, std::move(surface));
        panelSurfaceIds[panelId] = surfaceId;
        pendingRebuild           = true; // the main tree no longer contains this panel
        return surfaceId;
    }

    void EditorShell::SetFloatingGeometry(const std::string &panelId, float x, float y, float width, float height)
    {
        if (layoutModel != nullptr && layoutModel->SetFloatingGeometry(panelId, x, y, width, height)) {
            layoutDirty = true;
        }
    }

    void EditorShell::DockFloatingPanel(const std::string &panelId, const std::string &targetPanelId, DockPosition position)
    {
        const auto it = panelSurfaceIds.find(panelId);
        if (it == panelSurfaceIds.end()) {
            return;
        }
        const uint32_t surfaceId = it->second;
        panelSurfaceIds.erase(it);
        if (const auto sit = surfaces.find(surfaceId); sit != surfaces.end()) {
            Surface &surface = sit->second;
            if (surface.view != nullptr) {
                if (auto owned = surface.context->GetRoot()->RemoveChild(surface.view)) {
                    detachedViews[panelId] = std::move(owned);
                }
            }
            surfaces.erase(sit);
        }
        if (layoutModel != nullptr) {
            layoutModel->DockFloatingPanel(panelId, targetPanelId, position);
            layoutDirty = true;
        }
        pendingRebuild = true;
    }

    bool EditorShell::IsPanelFloating(const std::string &panelId) const
    {
        return panelSurfaceIds.find(panelId) != panelSurfaceIds.end();
    }

    uint32_t EditorShell::GetPanelSurface(const std::string &panelId) const
    {
        const auto it = panelSurfaceIds.find(panelId);
        return it != panelSurfaceIds.end() ? it->second : 0;
    }

    void EditorShell::PaintSurface(uint32_t surfaceId, sky::ui::UIPaintContext &paintContext, float inWidth, float inHeight)
    {
        // Each window has its own DPI scale; widgets read the global default
        // theme, so switch it to the surface's scale before painting.
        const float scale = GetSurfaceUiScale(surfaceId);
        if (scale != GetDefaultUiTheme().scale) {
            SetDefaultUiTheme(MakeDarkTheme(scale));
        }

        if (surfaceId == 0) {
            const std::weak_ptr<void> alive = actionLifetime;
            Layout(inWidth, inHeight);
            if (alive.expired())
                return;
            Paint(paintContext);
            return;
        }
        const auto it = surfaces.find(surfaceId);
        if (it == surfaces.end()) {
            return;
        }
        Surface &surface = it->second;
        if (SynchronizeDefaultUiTheme(*surface.context->GetRoot(), surface.themeRevision)) {
            ApplyTheme(surface.context.get());
        }
        surface.context->SetContentSize(inWidth, inHeight);
        if (surface.view != nullptr) {
            SynchronizePanelLayout(surface.view);
            surface.view->ArrangeIfNeeded(sky::ui::UIRect{0.0f, 0.0f, inWidth, inHeight});
        }
        surface.context->Paint(paintContext);
    }

    sky::ui::UIEventRouter *EditorShell::SurfaceRouter(uint32_t surfaceId)
    {
        const auto it = surfaces.find(surfaceId);
        return it != surfaces.end() ? it->second.router.get() : nullptr;
    }

    bool EditorShell::PrepareSurfaceInput(uint32_t surfaceId)
    {
        activeInputSurface              = surfaceId;
        const std::weak_ptr<void> alive = actionLifetime;
        const float               scale = GetSurfaceUiScale(surfaceId);
        if (surfaceId != 0 && surfaces.find(surfaceId) == surfaces.end()) {
            return false;
        }
        if (GetDefaultUiTheme().scale != scale) {
            SetDefaultUiTheme(MakeDarkTheme(scale));
        }
        if (surfaceId == 0) {
            // A tab press can queue a tree rebuild while its drag still owns
            // capture. Input must not consume that deferred rebuild mid-gesture.
            if (built && !pendingRebuild && mainThemeRevision != GetDefaultUiThemeRevision()) {
                Layout(width, height);
            }
            return !alive.expired();
        }
        const auto found         = surfaces.find(surfaceId);
        auto      *target        = found->second.context.get();
        auto      *view          = found->second.view;
        const auto rootReference = target->GetRoot()->GetInputReference();
        const auto viewReference = view != nullptr ? view->GetInputReference() : sky::ui::UIElement::InputReference{};
        if (SynchronizeDefaultUiTheme(*target->GetRoot(), found->second.themeRevision)) {
            ApplyTheme(target);
        }
        if (view != nullptr) {
            SynchronizePanelLayout(view);
        }
        if (alive.expired() || rootReference.Get() == nullptr || (view != nullptr && viewReference.Get() == nullptr)) {
            return false;
        }
        // Floating views receive a host-owned full-window slot, just as in
        // PaintSurface. Root AUTO layout must not replace it with Measure().
        if (view != nullptr) {
            view->ArrangeIfNeeded(target->GetRoot()->GetBounds());
        }
        return !alive.expired() && rootReference.Get() != nullptr && (view == nullptr || viewReference.Get() != nullptr);
    }

    bool EditorShell::DispatchPointerToSurface(uint32_t surfaceId, const sky::ui::UIPointerEvent &event)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (surfaceId == 0)
            return DispatchPointer(event);
        if (!PrepareSurfaceInput(surfaceId))
            return false;
        if (ActiveModal() != nullptr)
            return true;
        auto      *router  = SurfaceRouter(surfaceId);
        const bool handled = router != nullptr && router->DispatchPointer(event) == sky::ui::UIEventResult::HANDLED;
        if (!alive.expired())
            UpdateCommandContext(surfaceId);
        return handled;
    }

    bool EditorShell::DispatchKeyToSurface(uint32_t surfaceId, const sky::ui::UIKeyEvent &event)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (surfaceId == 0)
            return DispatchKey(event);
        if (!PrepareSurfaceInput(surfaceId))
            return false;
        if (ActiveModal() != nullptr)
            return true;
        if (const auto id = ActionBarBuilder::MatchAccelerator(event); !id.empty()) {
            if (!alive.expired())
                InvokeAction(id);
            return true;
        }
        if (alive.expired())
            return true;
        auto *router = SurfaceRouter(surfaceId);
        return router != nullptr && router->DispatchKey(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchTextToSurface(uint32_t surfaceId, const sky::ui::UITextInputEvent &event)
    {
        if (surfaceId == 0)
            return DispatchText(event);
        if (!PrepareSurfaceInput(surfaceId))
            return false;
        if (ActiveModal() != nullptr)
            return true;
        auto *router = SurfaceRouter(surfaceId);
        return router != nullptr && router->DispatchText(event) == sky::ui::UIEventResult::HANDLED;
    }

    std::vector<std::string> EditorShell::VisiblePanelIds() const
    {
        std::vector<std::string> present;
        if (layoutModel != nullptr) {
            layoutModel->CollectPanels(present);
        }
        return present;
    }

    void EditorShell::SetStatusInfo(const std::string &project, const std::string &rhi, const std::string &mode)
    {
        statusProject = project;
        statusRhi     = rhi;
        statusMode    = mode;
    }

    void EditorShell::SetFrameStats(float fps)
    {
        statusFps = fps;
    }

    void EditorShell::SetPlayState(PlayState state)
    {
        playState  = state;
        statusMode = state == PlayState::Playing ? "Play" : (state == PlayState::Paused ? "Pause" : "Edit");
    }

    sky::ui::UITextureId EditorShell::ResolveIcon(const std::string &name) const
    {
        const auto it = icons.find(name);
        return it != icons.end() ? it->second : sky::ui::UI_INVALID_TEXTURE;
    }

    void EditorShell::SetIcon(const std::string &name, sky::ui::UITextureId texture)
    {
        if (ResolveIcon(name) != texture)
            actionIconsDirty = true;
        icons[name] = texture;
        IconRegistry::Get()->Set(name, texture);
    }

    bool EditorShell::InvokeAction(const std::string &id)
    {
        return EditorActionRegistry::Get()->Invoke(id);
    }

    void EditorShell::ResetLayout()
    {
        if (layoutModel != nullptr) {
            layoutModel->ResetToDefault();
            pendingRebuild = true;
            layoutDirty    = true;
        }
    }

    void EditorShell::RefreshActions()
    {
        if (refreshingActions)
            return;
        const std::weak_ptr<void> alive      = actionLifetime;
        const uint64_t            generation = chromeGeneration;
        const auto                current    = [this, alive, generation]() { return !alive.expired() && generation == chromeGeneration; };
        refreshingActions                    = true;
        struct RefreshGuard {
            std::weak_ptr<void> alive;
            bool               *busy;
            ~RefreshGuard()
            {
                if (!alive.expired())
                    *busy = false;
            }
        } guard{alive, &refreshingActions};
        auto             *registry = EditorActionRegistry::Get();
        const uint64_t    revision = registry->GetRevision();
        const std::string scope    = EditorScopeStack::Get()->Active();
        if (actionChromeRevision != revision || actionChromeScope != scope) {
            const auto invoke = [this, alive](const std::string &id) {
                if (!alive.expired())
                    InvokeAction(id);
            };
            // Menus come from the action registry (via ActionBarBuilder) for the active scope:
            // actions are grouped by `menu` and may nest under a `submenu`. The host-only entries
            // (Preferences, and the dynamic panel toggles) are injected here.
            std::vector<MenuBar::Menu> menus = ActionBarBuilder::BuildMenus(invoke);
            if (!current())
                return;

            auto fileMenu = std::find_if(menus.begin(), menus.end(), [](const MenuBar::Menu &m) { return m.label == "File"; });
            if (fileMenu != menus.end()) {
                fileMenu->items.insert(fileMenu->items.begin(), {"Preferences...", [this]() { OpenPreferences(); }});
            }

            // Dynamic View > Windows panel toggles (panels are not registry actions); checkable rows.
            MenuBar::Item windowsItem;
            windowsItem.label = "Windows";
            if (layoutModel != nullptr && panelRegistry != nullptr) {
                for (const ViewMenuItem &entry : BuildViewMenuItems(*panelRegistry, *layoutModel)) {
                    const std::string panelId = entry.panelId;
                    const bool        shown   = entry.shown;
                    MenuBar::Item     toggle;
                    toggle.label     = entry.title;
                    toggle.checkable = true;
                    toggle.checked   = shown;
                    toggle.action    = [this, panelId, shown]() { SetPanelVisible(panelId, !shown); };
                    windowsItem.children.push_back(std::move(toggle));
                }
            }
            auto viewMenu = std::find_if(menus.begin(), menus.end(), [](const MenuBar::Menu &m) { return m.label == "View"; });
            if (viewMenu == menus.end()) {
                MenuBar::Menu view;
                view.label = "View";
                view.items.push_back(std::move(windowsItem));
                menus.push_back(std::move(view));
            } else {
                viewMenu->items.insert(viewMenu->items.begin(), std::move(windowsItem));
            }

            if (!current())
                return;
            const auto items = ActionBarBuilder::BuildToolBarItems(invoke);
            if (!current())
                return;
            if (auto *bar = static_cast<MenuBar *>(menuBarElement)) {
                bar->SetMenus(std::move(menus), true);
                if (!current())
                    return;
            }
            if (auto *bar = static_cast<ToolBar *>(toolBarElement))
                bar->SetItems(items);
            actionChromeRevision = revision;
            actionChromeScope    = scope;
            actionIconsDirty     = true;
        }
        if (auto *bar = static_cast<MenuBar *>(menuBarElement)) {
            if (!bar->RefreshActionStates() || !current())
                return;
        }
        if (auto *bar = static_cast<ToolBar *>(toolBarElement)) {
            if (!bar->RefreshActionStates() || !current())
                return;
            if (actionIconsDirty) {
                for (const auto &action : registry->GetToolbarActions()) {
                    if (!action.icon.empty())
                        bar->SetItemIcon(action.id, ResolveIcon(action.icon));
                }
                actionIconsDirty = false;
            }
        }
    }

    void EditorShell::SetDocumentInfo(const std::string &name, bool dirty)
    {
        documentName  = name;
        documentDirty = dirty;
        if (titleHandler) {
            titleHandler(FormatWindowTitle(documentName, documentDirty, statusProject));
        }
    }

    void EditorShell::CreateSplitters(const sky::ui::UIRect &contentRect)
    {
        splitterHandles.clear();
        splitterBands.clear();
        if (layoutModel == nullptr || layoutModel->IsEmpty()) {
            return;
        }
        const LayoutRect content{contentRect.left, contentRect.top, contentRect.right, contentRect.bottom};
        CollectSplitterBands(layoutModel->GetRoot(), content, kSplitterThickness, splitterBands);
        for (const auto &band : splitterBands) {
            auto handle = std::make_unique<SplitterHandle>();
            handle->SetBand(band);
            handle->SetOnDrag([this](const SplitterBand &b, float x, float y) {
                if (layoutModel != nullptr) {
                    layoutModel->SetRatio(b.split, b.index, RatioFromDrag(b, x, y));
                }
            });
            handle->SetOnDragEnd([this]() { layoutDirty = true; });
            splitterHandles.push_back(context->AddChild(std::move(handle)));
        }
        dropHighlight = context->AddChild(std::make_unique<DropHighlight>());
        dropHighlight->SetVisible(false);
        dragGhost = context->AddChild(std::make_unique<DragGhost>(textSystem));
        dragGhost->SetVisible(false);
    }

    void EditorShell::UpdateSplitters(const sky::ui::UIRect &contentRect)
    {
        if (splitterHandles.empty() || layoutModel == nullptr) {
            return;
        }
        const LayoutRect          content{contentRect.left, contentRect.top, contentRect.right, contentRect.bottom};
        std::vector<SplitterBand> bands;
        CollectSplitterBands(layoutModel->GetRoot(), content, kSplitterThickness, bands);
        if (bands.size() != splitterHandles.size()) {
            pendingRebuild = true;
            return;
        }
        splitterBands = bands;
        for (size_t i = 0; i < bands.size(); ++i) {
            if (auto *handle = dynamic_cast<SplitterHandle *>(splitterHandles[i])) {
                handle->SetBand(bands[i]);
                handle->Arrange(sky::ui::UIRect{bands[i].rect.left, bands[i].rect.top, bands[i].rect.right, bands[i].rect.bottom});
            }
        }
    }

    int EditorShell::FindSlotAt(float x, float y) const
    {
        for (size_t i = 0; i < slots.size(); ++i) {
            const auto &r = slots[i].rect;
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void EditorShell::BeginTabPress(const std::string &panelId, float x, float y)
    {
        dragPanel     = panelId;
        dragGhostText = panelId;
        if (panelRegistry != nullptr) {
            if (const PanelInfo *info = panelRegistry->Find(panelId)) {
                dragGhostText = info->title;
            }
        }
        dragActive = false;
        dragStartX = x;
        dragStartY = y;
    }

    void EditorShell::TabDragMove(float x, float y)
    {
        if (dragPanel.empty()) {
            return;
        }
        if (!dragActive) {
            const float dx = x - dragStartX;
            const float dy = y - dragStartY;
            if (dx * dx + dy * dy < 16.0f) {
                return;
            }
            dragActive = true;
        }
        UpdateDropHighlight(x, y);
        if (dragGhost != nullptr) {
            if (auto *ghost = dynamic_cast<DragGhost *>(dragGhost)) {
                ghost->SetText(dragGhostText);
            }
            const float ghostWidth = std::max(60.0f, uc::TextWidth(dragGhostText, 12, textSystem) + 20.0f);
            dragGhost->Arrange(sky::ui::UIRect{x + 12.0f, y + 12.0f, x + 12.0f + ghostWidth, y + 12.0f + 24.0f});
            dragGhost->SetVisible(true);
            dragGhost->MarkPaintDirty();
        }
    }

    void EditorShell::UpdateDropHighlight(float x, float y)
    {
        if (dropHighlight == nullptr) {
            return;
        }
        const int slotIndex = FindSlotAt(x, y);
        if (slotIndex < 0) {
            dropHighlight->SetVisible(false);
            return;
        }
        const Slot     &slot         = slots[static_cast<size_t>(slotIndex)];
        sky::ui::UIRect zone         = slot.rect;
        const float     headerBottom = slot.rect.top + kTabHeaderH;
        if (y >= headerBottom) {
            const LayoutRect body{slot.rect.left, headerBottom, slot.rect.right, slot.rect.bottom};
            const float      hw = body.Width() * 0.5f;
            const float      hh = body.Height() * 0.5f;
            switch (ResolveDockPosition(body, x, y, 0.25f)) {
            case DockPosition::LEFT: zone = {body.left, body.top, body.left + hw, body.bottom}; break;
            case DockPosition::RIGHT: zone = {body.left + hw, body.top, body.right, body.bottom}; break;
            case DockPosition::TOP: zone = {body.left, body.top, body.right, body.top + hh}; break;
            case DockPosition::BOTTOM: zone = {body.left, body.top + hh, body.right, body.bottom}; break;
            default: break;
            }
        }
        dropHighlight->Arrange(zone);
        dropHighlight->SetVisible(true);
    }

    void EditorShell::ApplyTabDrop(float x, float y)
    {
        if (layoutModel == nullptr || dragPanel.empty()) {
            return;
        }
        const int slotIndex = FindSlotAt(x, y);
        if (slotIndex < 0) {
            return;
        }
        const Slot &slot = slots[static_cast<size_t>(slotIndex)];
        if (slot.activePanel.empty() || slot.activePanel == dragPanel) {
            return;
        }
        DockPosition position     = DockPosition::CENTER;
        const float  headerBottom = slot.rect.top + kTabHeaderH;
        if (y >= headerBottom) {
            const LayoutRect body{slot.rect.left, headerBottom, slot.rect.right, slot.rect.bottom};
            position = ResolveDockPosition(body, x, y, 0.25f);
        }
        if (layoutModel->DockPanel(dragPanel, slot.activePanel, position)) {
            pendingRebuild = true;
            layoutDirty    = true;
        }
    }

    void EditorShell::TabDragEnd(float x, float y)
    {
        if (dragActive) {
            if (FindSlotAt(x, y) < 0) {
                pendingFloatPanel = dragPanel; // torn out of the dock area
            } else {
                ApplyTabDrop(x, y);
            }
        }
        dragPanel.clear();
        dragActive = false;
        if (dropHighlight != nullptr) {
            dropHighlight->SetVisible(false);
        }
        if (dragGhost != nullptr) {
            dragGhost->SetVisible(false);
        }
    }

    bool EditorShell::ConsumeFloatRequest(std::string &panelId)
    {
        if (pendingFloatPanel.empty()) {
            return false;
        }
        panelId = pendingFloatPanel;
        pendingFloatPanel.clear();
        return true;
    }

    sky::StandardCursor EditorShell::DesiredCursor(uint32_t surfaceId) const
    {
        sky::ui::UIEventRouter *router = nullptr;
        if (surfaceId == 0) {
            router = eventRouter.get();
        } else if (const auto it = surfaces.find(surfaceId); it != surfaces.end()) {
            router = it->second.router.get();
        }
        if (router == nullptr) {
            return sky::StandardCursor::Arrow;
        }
        if (auto *handle = dynamic_cast<SplitterHandle *>(router->GetHovered())) {
            return handle->IsHorizontal() ? sky::StandardCursor::ResizeHorizontal : sky::StandardCursor::ResizeVertical;
        }
        return sky::StandardCursor::Arrow;
    }

    void EditorShell::CreateNode(LayoutNode *node, const sky::ui::UIRect &rect)
    {
        if (node == nullptr) {
            return;
        }
        if (IsTab(node)) {
            auto *tab = static_cast<TabNode *>(node);
            if (tab->panels.empty()) {
                return;
            }
            std::vector<std::string> ids;
            std::vector<std::string> titles;
            for (const auto &tabPanel : tab->panels) {
                if (panelRegistry != nullptr && !panelRegistry->Contains(tabPanel.panelId)) {
                    continue;
                }
                ids.push_back(tabPanel.panelId);
                titles.push_back(panelRegistry != nullptr && panelRegistry->Find(tabPanel.panelId) != nullptr
                                     ? panelRegistry->Find(tabPanel.panelId)->title
                                     : tabPanel.panelId);
            }
            if (ids.empty()) {
                return;
            }
            int32_t index = tab->activeIndex;
            if (index < 0 || index >= static_cast<int32_t>(ids.size())) {
                index = 0;
            }

            Slot slot;
            slot.activePanel = ids[static_cast<size_t>(index)];
            slot.panels      = ids;
            {
                // Every panel gets a tab header (draggable + closable), including
                // single-panel areas (UE/Blender style).
                auto header = std::make_unique<TabHeader>(
                    titles, ids, index, textSystem,
                    [this, tab](int32_t idx) {
                        tab->activeIndex = idx;
                        pendingRebuild   = true;
                        layoutDirty      = true;
                    },
                    [this, ids](int32_t idx) {
                        if (layoutModel != nullptr && idx >= 0 && idx < static_cast<int32_t>(ids.size())) {
                            layoutModel->ClosePanel(ids[static_cast<size_t>(idx)]);
                            pendingRebuild = true;
                            layoutDirty    = true;
                        }
                    },
                    [this](const std::string &panelId, float x, float y) { BeginTabPress(panelId, x, y); },
                    [this](float x, float y) { TabDragMove(x, y); }, [this](float x, float y) { TabDragEnd(x, y); });
                slot.header = context->AddChild(std::move(header));
            }
            if (sky::ui::UIElement *body = CreatePanelView(ids[static_cast<size_t>(index)])) {
                slot.body = body; // already recorded in `panels` by AdoptView
                // The tab header shows the title, so hide the panel's own title bar.
                if (slot.header != nullptr) {
                    if (auto *chrome = dynamic_cast<IPanelChrome *>(body)) {
                        chrome->SetTitleBarVisible(false);
                    }
                }
            }
            slots.push_back(slot);
            return;
        }
        if (IsSplit(node)) {
            auto *split = static_cast<SplitNode *>(node);
            if (split->children.empty()) {
                return;
            }
            const auto slotsRect = SplitRect(split, rect);
            for (size_t i = 0; i < split->children.size(); ++i) {
                CreateNode(split->children[i].get(), slotsRect[i]);
            }
        }
    }

    void EditorShell::ApplyNode(LayoutNode *node, const sky::ui::UIRect &rect, size_t &panelCursor)
    {
        if (node == nullptr) {
            return;
        }
        if (IsTab(node)) {
            auto *tab = static_cast<TabNode *>(node);
            if (tab->panels.empty()) {
                return;
            }
            std::vector<std::string> ids;
            for (const auto &tabPanel : tab->panels) {
                if (panelRegistry != nullptr && !panelRegistry->Contains(tabPanel.panelId)) {
                    continue;
                }
                ids.push_back(tabPanel.panelId);
            }
            if (ids.empty()) {
                return;
            }
            if (panelCursor >= slots.size()) {
                return;
            }
            Slot &slot = slots[panelCursor++];
            slot.rect  = rect; // outer rect: drop zones / hit-testing
            // Inset the drawn panel so adjacent panels show a window-colored gap
            // (Blender-like separation).
            const float           gap = GetDefaultUiTheme().metrics.panelGap;
            const sky::ui::UIRect inner{rect.left + gap, rect.top + gap, rect.right - gap, rect.bottom - gap};
            sky::ui::UIRect       bodyRect = inner;
            if (slot.header != nullptr) {
                sky::ui::UIRect headerRect = inner;
                headerRect.bottom          = inner.top + kTabHeaderH;
                slot.header->Arrange(headerRect);
                bodyRect.top = headerRect.bottom;
            }
            if (slot.body != nullptr) {
                SynchronizePanelLayout(slot.body);
                slot.body->ArrangeIfNeeded(bodyRect);
            }
            return;
        }
        if (IsSplit(node)) {
            auto *split = static_cast<SplitNode *>(node);
            if (split->children.empty()) {
                return;
            }
            const auto slotsRect = SplitRect(split, rect);
            for (size_t i = 0; i < split->children.size(); ++i) {
                ApplyNode(split->children[i].get(), slotsRect[i], panelCursor);
            }
        }
    }

    void EditorShell::Rebuild()
    {
        const std::weak_ptr<void> alive      = actionLifetime;
        const uint64_t            generation = ++chromeGeneration;
        headerHeight                         = GetDefaultUiTheme().metrics.menuBarHeight;
        footerHeight                         = GetDefaultUiTheme().metrics.statusBarHeight;
        // Drop focus/hover/capture first: rebuilding destroys the chrome and
        // would leave the router holding dangling pointers.
        eventRouter->Reset();
        HarvestViews();
        // Scoped action callbacks remain registered while chrome is rebuilt.
        // Drop their current viewer target before its owning tree is destroyed.
        assetViewerElement = nullptr;
        context->GetRoot()->ClearChildren();
        menuBarElement   = nullptr;
        toolBarElement   = nullptr;
        statusBarElement = nullptr;
        slots.clear();
        built          = false;
        pendingRebuild = false;

        statusBarElement = context->AddChild(std::make_unique<StatusBar>(textSystem));

        const float dockTop = headerHeight + GetDefaultUiTheme().metrics.toolbarHeight;
        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            const sky::ui::UIRect rect{0.0f, dockTop, width, height - footerHeight};
            CreateNode(layoutModel->GetRoot(), rect);
        }

        dragPanel.clear();
        dragActive = false;
        CreateSplitters(sky::ui::UIRect{0.0f, dockTop, width, height - footerHeight});

        // Quick-action toolbar, built from the process-wide action registry (via
        // ActionBarBuilder) so plugins/extensions contribute entries (see editor-toolbar.md).
        auto toolBar   = std::make_unique<ToolBar>(textSystem);
        toolBarElement = context->AddChild(std::move(toolBar));

        // Menu bar is added LAST so it (and its popup) paints on top of the dock
        // content; otherwise panels cover the open menu.
        auto menuBar = std::make_unique<MenuBar>(std::vector<MenuBar::Menu>{}, textSystem);
        menuBar->SetBarHeight(headerHeight);
        menuBarElement = context->AddChild(std::move(menuBar));
        actionChromeScope.clear();

        // File browser overlay is added last so it paints above everything. It is
        // re-created on rebuild; if one was open, restore it.
        auto browser       = std::make_unique<FileBrowserDialog>(textSystem);
        fileBrowserElement = browser.get();
        browser->SetOnResult([this](const FileBrowserResult &result) {
            browserOpen   = false;
            auto callback = browserCallback;
            if (callback) {
                callback(result);
            }
        });
        context->AddChild(std::move(browser));
        if (browserOpen) {
            fileBrowserElement->Open(browserRequest);
        }

        // Asset viewer overlay (header + reserved preview + reflected cook settings).
        auto viewer        = std::make_unique<AssetViewerWidget>(textSystem);
        assetViewerElement = viewer.get();
        viewer->SetCatalog(assetCatalog);
        viewer->SetActionHandler([this](const std::string &id) { return InvokeAction(id); });
        viewer->SetOnClosed([this]() {
            if (assetViewerScope) {
                EditorScopeStack::Get()->Pop("assetViewer");
                assetViewerScope = false;
            }
            assetViewerOpen  = false;
            assetViewerAsset = Uuid{};
        });
        context->AddChild(std::move(viewer));
        if (assetViewerOpen) {
            assetViewerElement->Open(assetViewerAsset);
            if (alive.expired() || generation != chromeGeneration)
                return;
        }

        auto preferences   = std::make_unique<PreferencesDialog>(textSystem);
        preferencesElement = preferences.get();
        preferences->SetModel(preferenceRegistry);
        preferences->SetPersistence(preferencesPersistence);
        preferences->SetOnApplied([this]() {
            auto callback = preferencesApplied;
            if (callback) {
                callback();
            }
        });
        preferences->SetOnClosed([this]() { preferencesOpen = false; });
        context->AddChild(std::move(preferences));
        if (preferencesOpen) {
            preferencesElement->SetModel(preferenceRegistry);
            preferencesElement->Open();
        }

        auto newWorld   = std::make_unique<NewWorldDialog>(textSystem);
        newWorldElement = newWorld.get();
        newWorld->SetOnClosed([this]() { newWorldOpen = false; });
        context->AddChild(std::move(newWorld));
        if (newWorldOpen) {
            newWorldElement->SetOnCreate(newWorldCreate);
            newWorldElement->Open(newWorldLocation, newWorldName);
        }

        built = true;
        LOG_I(TAG, "editor shell built (%zu panels)", attachedViews.size());
        RefreshActions();
    }

    void EditorShell::SetPanelVisible(const std::string &panelId, bool visible)
    {
        if (layoutModel == nullptr) {
            return;
        }
        std::vector<std::string> present;
        layoutModel->CollectPanels(present);
        const bool isPresent = std::find(present.begin(), present.end(), panelId) != present.end();
        if (visible == isPresent) {
            return;
        }
        if (!visible) {
            layoutModel->ClosePanel(panelId);
        } else {
            std::string anchor = "viewport";
            if (std::find(present.begin(), present.end(), anchor) == present.end()) {
                anchor = present.empty() ? std::string() : present.front();
            }
            if (!anchor.empty()) {
                layoutModel->SplitPanel(anchor, SplitOrientation::VERTICAL, panelId);
            }
        }
        pendingRebuild = true;
        layoutDirty    = true;
    }

    void EditorShell::Layout(float inWidth, float inHeight)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        // Main-window callers also use Layout + Paint directly after floating
        // windows have temporarily selected their own DPI theme.
        if (GetDefaultUiTheme().scale != uiScale) {
            SetDefaultUiTheme(MakeDarkTheme(uiScale));
        }
        width        = inWidth > 0.0f ? inWidth : 1.0f;
        height       = inHeight > 0.0f ? inHeight : 1.0f;
        headerHeight = GetDefaultUiTheme().metrics.menuBarHeight;
        footerHeight = GetDefaultUiTheme().metrics.statusBarHeight;
        context->SetContentSize(width, height);

        if (pendingRebuild) {
            Rebuild();
            if (alive.expired())
                return;
        }
        if (SynchronizeDefaultUiTheme(*context->GetRoot(), mainThemeRevision)) {
            ApplyTheme();
        }

        RefreshActions();
        if (alive.expired())
            return;

        if (menuBarElement != nullptr) {
            auto *menuBar = static_cast<MenuBar *>(menuBarElement);
            menuBar->SetBarHeight(headerHeight);
            // Open menus own surface input, including tall submenus and outside-click dismissal.
            const float barBottom = menuBar->IsOpen() ? height : headerHeight;
            menuBarElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, barBottom});
        }

        const float toolBarHeight = GetDefaultUiTheme().metrics.toolbarHeight;
        if (toolBarElement != nullptr) {
            toolBarElement->Arrange(sky::ui::UIRect{0.0f, headerHeight, width, headerHeight + toolBarHeight});
        }

        if (statusBarElement != nullptr) {
            statusBarElement->Arrange(sky::ui::UIRect{0.0f, height - footerHeight, width, height});
            if (auto *statusBar = static_cast<StatusBar *>(statusBarElement)) {
                const std::size_t selectionCount = selection != nullptr ? selection->GetSelection().size() : 0;
                statusBar->SetText(interactionStatus.empty()
                                       ? FormatStatusBar(documentName, documentDirty, statusProject, statusMode, statusRhi, selectionCount, statusFps)
                                       : interactionStatus);
            }
        }

        const sky::ui::UIRect dockRect{0.0f, headerHeight + toolBarHeight, width, height - footerHeight};
        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            size_t cursor = 0;
            ApplyNode(layoutModel->GetRoot(), dockRect, cursor);
        }
        UpdateSplitters(dockRect);

        if (fileBrowserElement != nullptr) {
            fileBrowserElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
        }
        if (preferencesElement != nullptr) {
            preferencesElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
        }
        if (newWorldElement != nullptr) {
            newWorldElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
        }
        if (assetViewerElement != nullptr) {
            auto *viewer = assetViewerElement;
            viewer->SynchronizeLayout();
            if (alive.expired() || viewer != assetViewerElement)
                return;
            viewer->ArrangeIfNeeded(sky::ui::UIRect{0.0f, 0.0f, width, height});
        }
    }

    void EditorShell::OpenNewWorldDialog(const std::string &location, const std::string &name, std::function<void(const std::string &)> onCreate)
    {
        if (newWorldElement == nullptr) {
            return;
        }
        newWorldOpen     = true;
        newWorldLocation = location;
        newWorldName     = name;
        newWorldCreate   = onCreate;
        newWorldElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
        newWorldElement->SetOnCreate(newWorldCreate);
        newWorldElement->Open(location, name);
        newWorldElement->Arrange({0.0f, 0.0f, width, height});
    }

    void EditorShell::SetWorldDocument(WorldDocument *document)
    {
        const bool sameDocument = worldDocument == document;
        worldDocument           = document;
        const auto notify       = [document, sameDocument](sky::ui::UIElement *element) {
            if (auto *panel = dynamic_cast<WorldConfigPanel *>(element)) {
                panel->SetDocument(document);
                if (sameDocument) {
                    panel->RefreshDocument();
                }
            }
        };
        for (const auto &entry : attachedViews) {
            notify(entry.second);
        }
        for (const auto &entry : detachedViews) {
            notify(entry.second.get());
        }
        for (const auto &entry : surfaces) {
            notify(entry.second.view);
        }
    }

    void EditorShell::SetExportPresets(sky::ExportPresets *presets, std::string path)
    {
        exportPresets     = presets;
        exportPresetsPath = std::move(path);
    }

    void EditorShell::SetPreferences(PreferencePageRegistry *registry, std::function<void()> onApplied)
    {
        preferenceRegistry = registry;
        preferencesApplied = std::move(onApplied);
        if (preferencesElement != nullptr) {
            preferencesElement->SetModel(preferenceRegistry);
        }
    }

    void EditorShell::SetPreferencesPersistence(std::shared_ptr<PreferencesPersistence> persistence)
    {
        preferencesPersistence = std::move(persistence);
        if (preferencesElement != nullptr) {
            preferencesElement->SetPersistence(preferencesPersistence);
        }
    }

    void EditorShell::OpenPreferences()
    {
        if (preferencesElement == nullptr) {
            return;
        }
        const std::weak_ptr<void> alive      = actionLifetime;
        auto                     *dialog     = preferencesElement;
        const auto                reference  = dialog->GetInputReference();
        const auto                generation = chromeGeneration;
        const auto                current    = [this, alive, dialog, reference, generation] {
            return !alive.expired() && reference.Get() == dialog && chromeGeneration == generation && preferencesElement == dialog;
        };
        preferencesOpen = true;
        dialog->SetModel(preferenceRegistry);
        if (!current())
            return;
        dialog->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
        if (!current())
            return;
        dialog->Open();
        if (!current())
            return;
        dialog->Arrange({0.0f, 0.0f, width, height});
    }

    bool EditorShell::IsPreferencesOpen() const
    {
        return preferencesElement != nullptr && preferencesElement->IsOpen();
    }

    void EditorShell::OpenFileBrowser(const FileBrowserRequest &request, std::function<void(const FileBrowserResult &)> callback)
    {
        browserRequest  = request;
        browserCallback = std::move(callback);
        browserOpen     = true;
        if (fileBrowserElement != nullptr) {
            fileBrowserElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
            fileBrowserElement->Open(browserRequest);
        }
    }

    void EditorShell::OpenAssetViewer(const Uuid &asset)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (assetCatalog != nullptr) {
            EditorAssetItem item;
            if (assetCatalog->Find(asset, item) && !item.type.empty()) {
                std::string ext;
                if (const auto dot = item.path.find_last_of('.'); dot != std::string::npos) {
                    ext = item.path.substr(dot);
                }
                if (DeriveAssetViewFlags(item.type, ext) == AssetViewFlags::NONE) {
                    return; // HIDDEN: internal assets do not open the viewer
                }
            }
        }
        assetViewerAsset = asset;
        assetViewerOpen  = true;
        // Ensure the viewer's scoped actions (File/Edit + accelerators) exist, then make the
        // viewer scope active so the chrome resolves to them.
        RegisterAssetViewerActions();
        if (!assetViewerScope) {
            EditorScopeStack::Get()->Push("assetViewer");
            assetViewerScope = true;
        }
        if (assetViewerElement != nullptr) {
            assetViewerElement->SetActionHandler([this](const std::string &id) { return InvokeAction(id); });
            assetViewerElement->Arrange(sky::ui::UIRect{0.0f, 0.0f, width, height});
            auto *viewer = assetViewerElement;
            viewer->Open(asset);
            if (alive.expired() || viewer != assetViewerElement)
                return;
            viewer->Arrange({0.0f, 0.0f, width, height});
        }
    }

    void EditorShell::RegisterAssetViewerActions()
    {
        auto                     *actions    = EditorActionRegistry::Get();
        const std::string         scope      = "assetViewer";
        const std::weak_ptr<void> lifetime   = actionLifetime;
        const auto                ownerAlive = [lifetime]() { return !lifetime.expired(); };

        actions->Add({.id      = "assetViewer.open",
                      .label   = "Open",
                      .icon    = "open",
                      .menu    = EditorMenu::File,
                      .order   = 0,
                      .enabled = ownerAlive,
                      .invoke =
                          [this, lifetime]() {
                              if (!lifetime.expired() && assetViewerElement != nullptr) {
                                  assetViewerElement->OpenAsset();
                              }
                          },
                      .scope       = scope,
                      .accelerator = "Ctrl+O"});
        actions->Add({.id      = "assetViewer.save",
                      .label   = "Save",
                      .icon    = "save",
                      .group   = EditorGroup::File,
                      .menu    = EditorMenu::File,
                      .order   = 1,
                      .enabled = ownerAlive,
                      .invoke =
                          [this, lifetime]() {
                              if (!lifetime.expired() && assetViewerElement != nullptr) {
                                  assetViewerElement->SaveAsset();
                              }
                          },
                      .scope       = scope,
                      .accelerator = "Ctrl+S"});
        actions->Add({.id      = "assetViewer.close",
                      .label   = "Close",
                      .menu    = EditorMenu::File,
                      .order   = 2,
                      .enabled = ownerAlive,
                      .invoke =
                          [this, lifetime]() {
                              // Close must NOT save+cook: cooking is heavy work and running it inside
                              // the pointer/key dispatch blocks the UI (see coding rule 9). Saving is
                              // explicit (Save button / Ctrl+S).
                              if (!lifetime.expired() && assetViewerElement != nullptr) {
                                  assetViewerElement->Close();
                              }
                          },
                      .scope       = scope,
                      .accelerator = "Esc"});
        actions->Add({.id      = "assetViewer.undo",
                      .label   = "Undo",
                      .icon    = "undo",
                      .group   = EditorGroup::History,
                      .menu    = EditorMenu::Edit,
                      .order   = 0,
                      .enabled = [this, lifetime]() { return !lifetime.expired() && assetViewerElement != nullptr && assetViewerElement->CanUndo(); },
                      .invoke =
                          [this, lifetime]() {
                              if (!lifetime.expired() && assetViewerElement != nullptr) {
                                  assetViewerElement->Undo();
                              }
                          },
                      .scope       = scope,
                      .accelerator = "Ctrl+Z"});
        actions->Add({.id      = "assetViewer.redo",
                      .label   = "Redo",
                      .icon    = "redo",
                      .group   = EditorGroup::History,
                      .menu    = EditorMenu::Edit,
                      .order   = 1,
                      .enabled = [this, lifetime]() { return !lifetime.expired() && assetViewerElement != nullptr && assetViewerElement->CanRedo(); },
                      .invoke =
                          [this, lifetime]() {
                              if (!lifetime.expired() && assetViewerElement != nullptr) {
                                  assetViewerElement->Redo();
                              }
                          },
                      .scope       = scope,
                      .accelerator = "Ctrl+Y, Ctrl+Shift+Z"});
    }

    bool EditorShell::IsAssetViewerOpen() const
    {
        return assetViewerElement != nullptr && assetViewerElement->IsOpen();
    }

    void EditorShell::Paint(sky::ui::UIPaintContext &paintContext)
    {
        context->Paint(paintContext);
    }

    sky::ui::UIElement *EditorShell::HitTest(float x, float y) const
    {
        return eventRouter != nullptr ? eventRouter->HitTest(x, y) : nullptr;
    }

    sky::ui::UIElement *EditorShell::ActiveModal() const
    {
        if (newWorldElement != nullptr && newWorldElement->IsVisible()) {
            return newWorldElement;
        }
        if (preferencesElement != nullptr && preferencesElement->IsVisible()) {
            return preferencesElement;
        }
        if (fileBrowserElement != nullptr && fileBrowserElement->IsVisible()) {
            return fileBrowserElement;
        }
        if (assetViewerElement != nullptr && assetViewerElement->IsOpen()) {
            return assetViewerElement;
        }
        return nullptr;
    }

    bool EditorShell::DispatchPointer(const sky::ui::UIPointerEvent &event)
    {
        const std::weak_ptr<void> alive = actionLifetime;
        if (!PrepareSurfaceInput(0))
            return true;
        if (sky::ui::UIElement *modal = ActiveModal()) {
            modal->OnPointerEvent(event);
            return true;
        }
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        const bool overUI = eventRouter->HitTest(event.x, event.y) != nullptr;
        context->SetWantsInput(overUI || eventRouter->GetFocus() != nullptr);
        const bool handled = eventRouter->DispatchPointer(event) == sky::ui::UIEventResult::HANDLED;
        if (!alive.expired())
            UpdateCommandContext(0);
        return handled;
    }

    bool EditorShell::DispatchKey(const sky::ui::UIKeyEvent &event)
    {
        if (!PrepareSurfaceInput(0))
            return true;
        const std::weak_ptr<void> alive = actionLifetime;
        // Preferences owns an isolated editing session. Its history and modal
        // keys must reach that session before global actions and play shortcuts.
        if (preferencesElement != nullptr && preferencesElement->IsVisible()) {
            preferencesElement->OnKeyEvent(event);
            return true;
        }
        // Data-driven accelerators for the active scope (a dialog scope wins over global):
        // the same registry entries that drive the toolbar/menus are resolved here by `accelerator`.
        if (event.action == sky::ui::UIKeyAction::DOWN) {
            const std::string id = ActionBarBuilder::MatchAccelerator(event);
            if (alive.expired())
                return true;
            if (!id.empty()) {
                InvokeAction(id);
                return true;
            }
        }

        // F5 toggles Play/Pause (stateful -- it picks among play/play.stop by the current state,
        // so it is not a plain accelerator); Shift+F5 stops (UE convention).
        constexpr uint32_t kKeyF5 = 0x74; // VK_F5
        if (event.action == sky::ui::UIKeyAction::DOWN && event.keyCode == kKeyF5) {
            if ((event.modifiers & kModShift) != 0) {
                InvokeAction("play.stop");
            } else if (playState == PlayState::Playing) {
                InvokeAction("play.pause");
            } else {
                InvokeAction("play.play");
            }
            return true;
        }
        if (sky::ui::UIElement *modal = ActiveModal()) {
            modal->OnKeyEvent(event);
            return true;
        }
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        context->SetWantsInput(eventRouter->GetFocus() != nullptr);
        return eventRouter->DispatchKey(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchText(const sky::ui::UITextInputEvent &event)
    {
        if (!PrepareSurfaceInput(0))
            return true;
        if (sky::ui::UIElement *modal = ActiveModal()) {
            modal->OnTextInput(event);
            return true;
        }
        if (eventRouter == nullptr) {
            return false;
        }
        return eventRouter->DispatchText(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::WantsInput() const
    {
        if (ActiveModal() != nullptr) {
            return true;
        }
        return context != nullptr && context->WantsInput();
    }

    bool EditorShell::IsPanelInputActive(const std::string &panelId) const
    {
        if (ActiveModal() != nullptr || (menuBarElement && static_cast<MenuBar *>(menuBarElement)->IsOpen()))
            return false;
        sky::ui::UIElement     *view   = nullptr;
        sky::ui::UIEventRouter *router = nullptr;
        if (activeInputSurface == 0) {
            const auto found = attachedViews.find(panelId);
            if (found != attachedViews.end())
                view = found->second;
            router = eventRouter.get();
        } else {
            const auto found = surfaces.find(activeInputSurface);
            if (found != surfaces.end() && found->second.panelId == panelId) {
                view   = found->second.view;
                router = found->second.router.get();
            }
        }
        for (auto *focus = router ? router->GetFocus() : nullptr; focus; focus = focus->GetParent()) {
            if (focus == view)
                return true;
        }
        return false;
    }
    void EditorShell::ResetInput()
    {
        if (eventRouter)
            eventRouter->Reset();
        for (auto &[id, surface] : surfaces) {
            if (surface.router)
                surface.router->Reset();
        }
    }
    bool EditorShell::HasPointerCapture(uint32_t surfaceId) const
    {
        const auto  found  = surfaces.find(surfaceId);
        const auto *router = surfaceId == 0 ? eventRouter.get() : found != surfaces.end() ? found->second.router.get() : nullptr;
        return router != nullptr && router->GetCapture() != nullptr;
    }
    void EditorShell::SetPanelCommands(const std::string &panelId, CommandService *commands)
    {
        if (commands)
            panelCommands[panelId] = commands;
        else
            panelCommands.erase(panelId);
    }
    void EditorShell::UpdateCommandContext(uint32_t surfaceId)
    {
        const auto *router = surfaceId == 0 ? eventRouter.get() : SurfaceRouter(surfaceId);
        if (!router || !router->GetFocus() || ActiveModal())
            return;
        activeCommandPanel.clear();
        for (const auto &[panelId, commands] : panelCommands) {
            if (IsPanelInputActive(panelId)) {
                activeCommandPanel = panelId;
                return;
            }
        }
    }
    CommandService &EditorShell::ActiveCommands() const
    {
        const auto found = panelCommands.find(activeCommandPanel);
        const bool attached =
            attachedViews.find(activeCommandPanel) != attachedViews.end() ||
            std::any_of(surfaces.begin(), surfaces.end(), [this](const auto &entry) { return entry.second.panelId == activeCommandPanel; });
        return found == panelCommands.end() || !attached ? EditorCore::GetCommandService() : *found->second;
    }

    bool EditorShell::HasPanel(const std::string &panelId) const
    {
        return attachedViews.find(panelId) != attachedViews.end();
    }

} // namespace sky::editor
