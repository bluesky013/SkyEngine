//
// Created on 2026/09/22.
//

#include <editor/shell/EditorShell.h>
#include <editor/core/layout/DockInteraction.h>
#include <editor/core/shell/ShellModels.h>
#include <editor/shell/PanelView.h>
#include <editor/shell/ReflectedConfigPanel.h>
#include <editor/shell/ReflectedInspectorPanel.h>
#include <editor/shell/ReflectionDemoPanel.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>
#include <editor/shell/widgets/MenuBar.h>
#include <editor/shell/widgets/StatusBar.h>
#include <editor/shell/widgets/DockWidgets.h>
#include <editor/shell/panels/ServicePanels.h>

#include <ui/UIElement.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <core/logger/Logger.h>

static const char *TAG = "EditorShell";

namespace sky::editor {

    namespace {

        namespace uc = uidraw;



        // Child rectangles of a split, via the shared core geometry helper.
        std::vector<sky::ui::UIRect> SplitRect(LayoutNode *node, const sky::ui::UIRect &rect)
        {
            std::vector<LayoutRect> children;
            ComputeChildRects(*static_cast<SplitNode *>(node), LayoutRect{rect.left, rect.top, rect.right, rect.bottom},
                              children);
            std::vector<sky::ui::UIRect> out;
            out.reserve(children.size());
            for (const LayoutRect &child : children) {
                out.push_back(sky::ui::UIRect{child.left, child.top, child.right, child.bottom});
            }
            return out;
        }
    } // namespace

    EditorShell::EditorShell()
        : context(std::make_unique<sky::ui::UIContext>())
    {
        eventRouter = std::make_unique<sky::ui::UIEventRouter>(*context);
        ApplyTheme();
    }

    EditorShell::~EditorShell() = default;

    void EditorShell::ApplyTheme()
    {
        sky::ui::UITheme &theme = context->GetTheme();

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

    void EditorShell::SetTextSystem(sky::ui::UITextSystem *text) { textSystem = text; }
    void EditorShell::SetLayout(LayoutModel *layout) { layoutModel = layout; }
    void EditorShell::SetPanelRegistry(PanelRegistry *registry) { panelRegistry = registry; }
    void EditorShell::SetSelection(SelectionService *value) { selection = value; }
    void EditorShell::SetLogService(LogService *value) { logService = value; }
    void EditorShell::SetCommandController(CommandController *value) { commandController = value; }
    void EditorShell::SetInspectorModel(PropertyModel *model) { inspectorModel = model; }

    void EditorShell::SetPropertySource(IEditorPropertySource *source) { propertySource = source; }

    void EditorShell::SetConfigSource(IEditorConfigSource *source) { configSource = source; }

    void EditorShell::SetUiScale(float scale)
    {
        // Scale the theme metrics/fonts by the DPI ratio so the UI is drawn at
        // physical size 1:1 (crisp) instead of upscaling a logical layout.
        uiScale = scale;
        SetDefaultUiTheme(MakeDarkTheme(scale));
        lastThemeScale = scale;
    }

    void EditorShell::SetSurfaceScale(uint32_t surfaceId, float scale)
    {
        surfaceScales[surfaceId] = scale;
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

        RegisterPanelView("viewport", [this, t = titleOf("viewport", "Viewport")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ShellPanel(t));
        });
        RegisterPanelView("outliner", [this, t = titleOf("outliner", "Outliner")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutlinerPanel(selection, textSystem, t));
        });
        RegisterPanelView("inspector", [this, t = titleOf("inspector", "Inspector")]() {
            return std::unique_ptr<sky::ui::UIElement>(
                new ReflectedInspectorPanel(textSystem, propertySource, selection, t));
        });
        RegisterPanelView("config", [this, t = titleOf("config", "Config")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ReflectedConfigPanel(textSystem, configSource, t));
        });
        RegisterPanelView("refldemo", [this, t = titleOf("refldemo", "Reflection Demo")]() {
            return CreateReflectionDemoPanel(textSystem, t);
        });
        RegisterPanelView("outputlog", [this, t = titleOf("outputlog", "Output Log")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutputLogPanel(logService, textSystem, t));
        });
        RegisterPanelView("console", [this, t = titleOf("console", "Console")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ConsolePanel(commandController, textSystem, t));
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
        const auto it = viewFactories.find(panelId);
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
        sky::ui::UIElement *raw = context->AddChild(std::move(element));
        attachedViews[panelId] = raw;
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
        if (const auto it = attachedViews.find(panelId); it != attachedViews.end()) {
            return it->second;
        }
        if (const auto it = detachedViews.find(panelId); it != detachedViews.end()) {
            return it->second.get();
        }
        std::unique_ptr<sky::ui::UIElement> element = MakePanelView(panelId);
        sky::ui::UIElement *raw = element.get();
        detachedViews[panelId] = std::move(element);
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
        layoutDirty = false;
        return dirty;
    }

    uint32_t EditorShell::FloatPanelToSurface(const std::string &panelId, uint32_t surfaceId,
                                              const FloatingPanel &geometry)
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
        surface.context = std::make_unique<sky::ui::UIContext>();
        surface.router = std::make_unique<sky::ui::UIEventRouter>(*surface.context);
        surface.context->GetTheme() = context->GetTheme();
        surface.panelId = panelId;

        std::unique_ptr<sky::ui::UIElement> view = TakePanelView(panelId);
        if (view == nullptr) {
            view = MakePanelView(panelId);
        }
        surface.view = surface.context->AddChild(std::move(view));
        surface.view->SetBounds(sky::ui::UIRect{0.0f, 0.0f, width, height});

        surfaces.emplace(surfaceId, std::move(surface));
        panelSurfaceIds[panelId] = surfaceId;
        pendingRebuild = true; // the main tree no longer contains this panel
        return surfaceId;
    }

    void EditorShell::SetFloatingGeometry(const std::string &panelId, float x, float y, float width, float height)
    {
        if (layoutModel != nullptr && layoutModel->SetFloatingGeometry(panelId, x, y, width, height)) {
            layoutDirty = true;
        }
    }

    void EditorShell::DockFloatingPanel(const std::string &panelId, const std::string &targetPanelId,
                                        DockPosition position)
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

    void EditorShell::PaintSurface(uint32_t surfaceId, sky::ui::UIPaintContext &paintContext, float inWidth,
                                   float inHeight)
    {
        // Each window has its own DPI scale; widgets read the global default
        // theme, so switch it to the surface's scale before painting.
        float scale = uiScale;
        if (surfaceId != 0) {
            if (const auto it = surfaceScales.find(surfaceId); it != surfaceScales.end()) {
                scale = it->second;
            }
        }
        if (scale != lastThemeScale) {
            SetDefaultUiTheme(MakeDarkTheme(scale));
            lastThemeScale = scale;
        }

        if (surfaceId == 0) {
            Layout(inWidth, inHeight);
            Paint(paintContext);
            return;
        }
        const auto it = surfaces.find(surfaceId);
        if (it == surfaces.end()) {
            return;
        }
        Surface &surface = it->second;
        surface.context->SetContentSize(inWidth, inHeight);
        if (surface.view != nullptr) {
            surface.view->SetBounds(sky::ui::UIRect{0.0f, 0.0f, inWidth, inHeight});
        }
        surface.context->Paint(paintContext);
    }

    sky::ui::UIEventRouter *EditorShell::SurfaceRouter(uint32_t surfaceId)
    {
        const auto it = surfaces.find(surfaceId);
        return it != surfaces.end() ? it->second.router.get() : nullptr;
    }

    bool EditorShell::DispatchPointerToSurface(uint32_t surfaceId, const sky::ui::UIPointerEvent &event)
    {
        auto *router = SurfaceRouter(surfaceId);
        return router != nullptr && router->DispatchPointer(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchKeyToSurface(uint32_t surfaceId, const sky::ui::UIKeyEvent &event)
    {
        auto *router = SurfaceRouter(surfaceId);
        return router != nullptr && router->DispatchKey(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchTextToSurface(uint32_t surfaceId, const sky::ui::UITextInputEvent &event)
    {
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
        statusRhi = rhi;
        statusMode = mode;
    }

    void EditorShell::SetFrameStats(float fps) { statusFps = fps; }

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
        const LayoutRect content{contentRect.left, contentRect.top, contentRect.right, contentRect.bottom};
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
                handle->SetBounds(sky::ui::UIRect{bands[i].rect.left, bands[i].rect.top, bands[i].rect.right,
                                                  bands[i].rect.bottom});
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
        dragPanel = panelId;
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
            dragGhost->SetBounds(
                sky::ui::UIRect{x + 12.0f, y + 12.0f, x + 12.0f + ghostWidth, y + 12.0f + 24.0f});
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
        const Slot &slot = slots[static_cast<size_t>(slotIndex)];
        sky::ui::UIRect zone = slot.rect;
        const float headerBottom = slot.rect.top + kTabHeaderH;
        if (y >= headerBottom) {
            const LayoutRect body{slot.rect.left, headerBottom, slot.rect.right, slot.rect.bottom};
            const float hw = body.Width() * 0.5f;
            const float hh = body.Height() * 0.5f;
            switch (ResolveDockPosition(body, x, y, 0.25f)) {
                case DockPosition::LEFT:   zone = {body.left, body.top, body.left + hw, body.bottom}; break;
                case DockPosition::RIGHT:  zone = {body.left + hw, body.top, body.right, body.bottom}; break;
                case DockPosition::TOP:    zone = {body.left, body.top, body.right, body.top + hh}; break;
                case DockPosition::BOTTOM: zone = {body.left, body.top + hh, body.right, body.bottom}; break;
                default: break;
            }
        }
        dropHighlight->SetBounds(zone);
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
        DockPosition position = DockPosition::CENTER;
        const float headerBottom = slot.rect.top + kTabHeaderH;
        if (y >= headerBottom) {
            const LayoutRect body{slot.rect.left, headerBottom, slot.rect.right, slot.rect.bottom};
            position = ResolveDockPosition(body, x, y, 0.25f);
        }
        if (layoutModel->DockPanel(dragPanel, slot.activePanel, position)) {
            pendingRebuild = true;
            layoutDirty = true;
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
            return handle->IsHorizontal() ? sky::StandardCursor::ResizeHorizontal
                                          : sky::StandardCursor::ResizeVertical;
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
            slot.panels = ids;
            {
                // Every panel gets a tab header (draggable + closable), including
                // single-panel areas (UE/Blender style).
                auto header = std::make_unique<TabHeader>(
                    titles, ids, index, textSystem,
                    [this, tab](int32_t idx) {
                        tab->activeIndex = idx;
                        pendingRebuild = true;
                        layoutDirty = true;
                    },
                    [this, ids](int32_t idx) {
                        if (layoutModel != nullptr && idx >= 0 && idx < static_cast<int32_t>(ids.size())) {
                            layoutModel->ClosePanel(ids[static_cast<size_t>(idx)]);
                            pendingRebuild = true;
                            layoutDirty = true;
                        }
                    },
                    [this](const std::string &panelId, float x, float y) { BeginTabPress(panelId, x, y); },
                    [this](float x, float y) { TabDragMove(x, y); },
                    [this](float x, float y) { TabDragEnd(x, y); });
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
            slot.rect = rect; // outer rect: drop zones / hit-testing
            // Inset the drawn panel so adjacent panels show a window-colored gap
            // (Blender-like separation).
            const float gap = GetDefaultUiTheme().metrics.panelGap;
            const sky::ui::UIRect inner{rect.left + gap, rect.top + gap, rect.right - gap, rect.bottom - gap};
            sky::ui::UIRect bodyRect = inner;
            if (slot.header != nullptr) {
                sky::ui::UIRect headerRect = inner;
                headerRect.bottom = inner.top + kTabHeaderH;
                slot.header->SetBounds(headerRect);
                bodyRect.top = headerRect.bottom;
            }
            if (slot.body != nullptr) {
                slot.body->SetBounds(bodyRect);
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
        // Drop focus/hover/capture first: rebuilding destroys the chrome and
        // would leave the router holding dangling pointers.
        eventRouter->Reset();
        HarvestViews();
        context->GetRoot()->ClearChildren();
        menuBarElement = nullptr;
        statusBarElement = nullptr;
        slots.clear();
        built = false;
        pendingRebuild = false;

        // Menu bar: File/Edit/View/Window/Tools/Help. View exposes panel
        // visibility, Window exposes Reset Layout.
        std::vector<MenuBar::Menu> menus;
        MenuBar::Menu file;
        file.label = "File";
        file.items.push_back({"Quit", []() { LOG_I(TAG, "quit requested"); }});
        menus.push_back(std::move(file));

        MenuBar::Menu edit;
        edit.label = "Edit";
        edit.items.push_back({"Undo", []() {}});
        menus.push_back(std::move(edit));

        MenuBar::Menu view{"View", {}};
        if (layoutModel != nullptr && panelRegistry != nullptr) {
            for (const ViewMenuItem &entry : BuildViewMenuItems(*panelRegistry, *layoutModel)) {
                const std::string panelId = entry.panelId;
                const bool shown = entry.shown;
                view.items.push_back({(shown ? "Hide " : "Show ") + entry.title,
                                      [this, panelId, shown]() { SetPanelVisible(panelId, !shown); }});
            }
        }
        menus.push_back(std::move(view));
        menus.push_back({"Window", {{"Reset Layout", [this]() {
                                        if (layoutModel != nullptr) {
                                            layoutModel->ResetToDefault();
                                            pendingRebuild = true;
                                            layoutDirty = true;
                                        }
                                    }}}});
        menus.push_back({"Tools", {{"About", []() { LOG_I(TAG, "SkyEngine Editor (sandbox shell)"); }}}});
        MenuBar::Menu help;
        help.label = "Help";
        help.items.push_back({"Demo", [this]() { SetPanelVisible("refldemo", true); }});
        help.items.push_back({"About", []() { LOG_I(TAG, "SkyEngine Editor"); }});
        menus.push_back(std::move(help));

        statusBarElement = context->AddChild(std::make_unique<StatusBar>(textSystem));

        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            const sky::ui::UIRect rect{0.0f, headerHeight, width, height - footerHeight};
            CreateNode(layoutModel->GetRoot(), rect);
        }

        dragPanel.clear();
        dragActive = false;
        CreateSplitters(sky::ui::UIRect{0.0f, headerHeight, width, height - footerHeight});

        // Menu bar is added LAST so it (and its popup) paints on top of the dock
        // content; otherwise panels cover the open menu.
        auto menuBar = std::make_unique<MenuBar>(std::move(menus), textSystem);
        menuBar->SetBarHeight(headerHeight);
        menuBarElement = context->AddChild(std::move(menuBar));

        built = true;
        LOG_I(TAG, "editor shell built (%zu panels)", attachedViews.size());
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
        layoutDirty = true;
    }

    void EditorShell::Layout(float inWidth, float inHeight)
    {
        width  = inWidth > 0.0f ? inWidth : 1.0f;
        height = inHeight > 0.0f ? inHeight : 1.0f;
        context->SetContentSize(width, height);

        if (pendingRebuild) {
            Rebuild();
        }

        if (menuBarElement != nullptr) {
            float barBottom = headerHeight;
            if (auto *menuBar = dynamic_cast<MenuBar *>(menuBarElement); menuBar != nullptr && menuBar->IsOpen()) {
                barBottom += menuBar->PopupHeight();
            }
            menuBarElement->SetBounds(sky::ui::UIRect{0.0f, 0.0f, width, barBottom});
        }

        if (statusBarElement != nullptr) {
            statusBarElement->SetBounds(sky::ui::UIRect{0.0f, height - footerHeight, width, height});
            if (auto *statusBar = dynamic_cast<StatusBar *>(statusBarElement)) {
                const std::size_t selectionCount = selection != nullptr ? selection->GetSelection().size() : 0;
                statusBar->SetText(FormatStatusBar(statusProject, statusMode, statusRhi, selectionCount, statusFps));
            }
        }

        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            const sky::ui::UIRect rect{0.0f, headerHeight, width, height - footerHeight};
            size_t             cursor = 0;
            ApplyNode(layoutModel->GetRoot(), rect, cursor);
        }
        UpdateSplitters(sky::ui::UIRect{0.0f, headerHeight, width, height - footerHeight});
    }

    void EditorShell::Paint(sky::ui::UIPaintContext &paintContext)
    {
        context->Paint(paintContext);
    }

    sky::ui::UIElement *EditorShell::HitTest(float x, float y) const
    {
        return eventRouter != nullptr ? eventRouter->HitTest(x, y) : nullptr;
    }

    bool EditorShell::DispatchPointer(const sky::ui::UIPointerEvent &event)
    {
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        const bool overUI = eventRouter->HitTest(event.x, event.y) != nullptr;
        context->SetWantsInput(overUI || eventRouter->GetFocus() != nullptr);
        return eventRouter->DispatchPointer(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchKey(const sky::ui::UIKeyEvent &event)
    {
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        context->SetWantsInput(eventRouter->GetFocus() != nullptr);
        return eventRouter->DispatchKey(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchText(const sky::ui::UITextInputEvent &event)
    {
        if (eventRouter == nullptr) {
            return false;
        }
        return eventRouter->DispatchText(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::WantsInput() const
    {
        return context != nullptr && context->WantsInput();
    }

    bool EditorShell::HasPanel(const std::string &panelId) const
    {
        return attachedViews.find(panelId) != attachedViews.end();
    }

} // namespace sky::editor
