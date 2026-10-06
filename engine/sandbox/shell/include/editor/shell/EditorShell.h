//
// Created on 2026/09/22.
//

#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/console/CommandController.h>
#include <editor/core/filebrowser/FileBrowserTypes.h>
#include <editor/core/layout/DockInteraction.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/log/LogService.h>
#include <editor/core/property/EditorPropertySource.h>
#include <editor/core/property/PropertyModel.h>
#include <editor/core/selection/SelectionService.h>
#include <framework/window/Cursor.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::ui {
    class UIElement;
    class UIContext;
    class UIPaintContext;
    class UIEventRouter;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class FileBrowserDialog;

    // UI-linked editor shell.
    //
    // Composes a `sky::ui` element tree from the render-independent layout model
    // and panel registry, hosts the built-in panel views, and paints them. It
    // owns no render/RHI state; the editor renderer just draws the produced draw
    // data.
    class EditorShell {
    public:
        using PanelViewFactory = std::function<std::unique_ptr<sky::ui::UIElement>()>;

        EditorShell();
        ~EditorShell();

        EditorShell(const EditorShell &)            = delete;
        EditorShell &operator=(const EditorShell &) = delete;

        // Services the shell reads from (owned by the editor host).
        void SetTextSystem(sky::ui::UITextSystem *text);
        void SetLayout(LayoutModel *layout);
        void SetPanelRegistry(PanelRegistry *registry);
        void SetSelection(SelectionService *selection);
        void SetLogService(LogService *log);
        void SetCommandController(CommandController *console);
        // Optional: the object the Inspector renders. Null shows an empty state.
        void SetInspectorModel(PropertyModel *model);

        // Optional: resolves the current selection to reflected data for the inspector.
        void SetPropertySource(IEditorPropertySource *source);

        // Optional: supplies named reflected configurations for the config panel.
        void SetConfigSource(IEditorConfigSource *source);

        // Opens the reusable file browser as a modal over the shell. The callback
        // receives the result on accept/cancel. While open, the dialog captures
        // pointer/keyboard/text input.
        void OpenFileBrowser(const FileBrowserRequest &request, std::function<void(const FileBrowserResult &)> callback);
        bool IsFileBrowserOpen() const
        {
            return browserOpen;
        }

        // DPI/UI scale: layout stays logical, painting scales to physical pixels.
        void SetUiScale(float scale);

        // Status-bar values resolved by the host (project/engine/RHI/mode/fps).
        void SetStatusInfo(const std::string &project, const std::string &rhi, const std::string &mode);
        void SetFrameStats(float fps);

        // Registers a view factory for a panel id. Falls back to a titled frame
        // when no factory is registered.
        void RegisterPanelView(const std::string &panelId, PanelViewFactory factory);

        // Registers the built-in panel views for the default panel ids.
        void RegisterBuiltinPanelViews();

        // (Re)builds the element tree from the current layout + registry. Panel
        // views are preserved across rebuilds through the view registry.
        void Rebuild();
        bool IsBuilt() const
        {
            return built;
        }

        // Sets the surface size and re-applies panel rectangles.
        void Layout(float width, float height);
        void Paint(sky::ui::UIPaintContext &context);

        // Input entry points (device pixels). The host maps platform events to
        // these and forwards them; the shell dispatches through the UI event
        // router. Returns whether the event was handled.
        bool                DispatchPointer(const sky::ui::UIPointerEvent &event);
        bool                DispatchKey(const sky::ui::UIKeyEvent &event);
        bool                DispatchText(const sky::ui::UITextInputEvent &event);
        sky::ui::UIElement *HitTest(float x, float y) const;

        // Whether the UI wants input (used to gate viewport input).
        bool WantsInput() const;

        size_t GetPanelCount() const
        {
            return attachedViews.size();
        }
        bool HasPanel(const std::string &panelId) const;
        // Toggles a panel's visibility through the layout model and rebuilds.
        void SetPanelVisible(const std::string &panelId, bool visible);

        // View registry: returns the view for a panel, creating it if needed.
        // The returned view may be detached (floating/hidden).
        sky::ui::UIElement *GetPanelView(const std::string &panelId);
        // Detaches a panel view and transfers ownership (used when moving a panel
        // into a floating window's UI context).
        std::unique_ptr<sky::ui::UIElement> TakePanelView(const std::string &panelId);
        bool                                IsPanelAttached(const std::string &panelId) const;

        // True once since the last call if the layout was mutated by a committed
        // edit (dock/close/float/ratio/reset). The host auto-saves on this.
        bool ConsumeLayoutDirty();

        // Floating surfaces: a panel rendered in its own window's UI context
        // (surfaceId 0 is the main window). Paint/input are dispatched per surface.
        uint32_t FloatPanelToSurface(const std::string &panelId, uint32_t surfaceId, const FloatingPanel &geometry);
        // Binds a panel that is already in the model's floating set (startup
        // restore) to a surface, without re-floating it.
        uint32_t RestoreFloatingPanel(const std::string &panelId, uint32_t surfaceId);
        void     DockFloatingPanel(const std::string &panelId, const std::string &targetPanelId, DockPosition position);
        void     SetFloatingGeometry(const std::string &panelId, float x, float y, float width, float height);
        void     PaintSurface(uint32_t surfaceId, sky::ui::UIPaintContext &context, float width, float height);
        bool     DispatchPointerToSurface(uint32_t surfaceId, const sky::ui::UIPointerEvent &event);
        bool     DispatchKeyToSurface(uint32_t surfaceId, const sky::ui::UIKeyEvent &event);
        bool     DispatchTextToSurface(uint32_t surfaceId, const sky::ui::UITextInputEvent &event);
        bool     IsPanelFloating(const std::string &panelId) const;
        uint32_t GetPanelSurface(const std::string &panelId) const;

        // Cursor the OS should show for a surface (0 = main), based on what is
        // hovered (e.g. a resize cursor over a splitter).
        sky::StandardCursor DesiredCursor(uint32_t surfaceId = 0) const;

        // Per-window DPI scale for a floating surface (its UI is themed/scaled
        // independently from the main window).
        void SetSurfaceScale(uint32_t surfaceId, float scale);
        // True once if a tab was torn out (dragged outside the dock area); the
        // host creates the surface and calls FloatPanelToSurface. Outputs the id.
        bool ConsumeFloatRequest(std::string &panelId);

    private:
        // One layout tab: an optional header row plus the active panel's body.
        struct Slot {
            sky::ui::UIElement      *header = nullptr;
            sky::ui::UIElement      *body   = nullptr;
            sky::ui::UIRect          rect;        // tab area (header + body)
            std::string              activePanel; // panel currently shown
            std::vector<std::string> panels;      // filtered panel ids in tab order
        };

        // One floating window's UI: its own context + router + the panel view.
        struct Surface {
            std::unique_ptr<sky::ui::UIContext>     context;
            std::unique_ptr<sky::ui::UIEventRouter> router;
            std::string                             panelId;
            sky::ui::UIElement                     *view = nullptr;
        };

        // Event router for a floating surface (>0). Null if unknown; surface 0
        // uses the main router through the public Dispatch* methods.
        sky::ui::UIEventRouter *SurfaceRouter(uint32_t surfaceId);
        // Creates a surface's UI context + router and moves the panel view into
        // it. Shared by float (tear-out) and startup restore.
        uint32_t CreateSurfaceContext(const std::string &panelId, uint32_t surfaceId);

        void                                ApplyTheme();
        void                                CreateNode(LayoutNode *node, const sky::ui::UIRect &rect);
        void                                ApplyNode(LayoutNode *node, const sky::ui::UIRect &rect, size_t &panelCursor);
        sky::ui::UIElement                 *CreatePanelView(const std::string &panelId);
        std::unique_ptr<sky::ui::UIElement> MakePanelView(const std::string &panelId);
        sky::ui::UIElement                 *AdoptView(const std::string &panelId, std::unique_ptr<sky::ui::UIElement> element);
        void                                HarvestViews();
        std::vector<std::string>            VisiblePanelIds() const;

        // Splitter + tab-drag interaction.
        void CreateSplitters(const sky::ui::UIRect &contentRect);
        void UpdateSplitters(const sky::ui::UIRect &contentRect);
        void BeginTabPress(const std::string &panelId, float x, float y);
        void TabDragMove(float x, float y);
        void TabDragEnd(float x, float y);
        void UpdateDropHighlight(float x, float y);
        void ApplyTabDrop(float x, float y);
        int  FindSlotAt(float x, float y) const;

        std::unique_ptr<sky::ui::UIContext>     context;
        std::unique_ptr<sky::ui::UIEventRouter> eventRouter;
        LayoutModel                            *layoutModel       = nullptr;
        PanelRegistry                          *panelRegistry     = nullptr;
        sky::ui::UITextSystem                  *textSystem        = nullptr;
        SelectionService                       *selection         = nullptr;
        LogService                             *logService        = nullptr;
        CommandController                      *commandController = nullptr;
        PropertyModel                          *inspectorModel    = nullptr;
        IEditorPropertySource                  *propertySource    = nullptr;
        IEditorConfigSource                    *configSource      = nullptr;
        float                                   uiScale           = 1.0f;

        std::unordered_map<std::string, PanelViewFactory> viewFactories;
        // View registry: views not currently attached to the main context. Views
        // attached to the main context are owned by it; this pool holds the rest
        // (floating/hidden) so identity survives Rebuild.
        std::unordered_map<std::string, std::unique_ptr<sky::ui::UIElement>> detachedViews;
        std::unordered_map<std::string, sky::ui::UIElement *>                attachedViews;

        std::unordered_map<uint32_t, Surface>     surfaces;        // surfaceId -> floating UI
        std::unordered_map<std::string, uint32_t> panelSurfaceIds; // panelId -> surfaceId
        std::unordered_map<uint32_t, float>       surfaceScales;   // surfaceId -> DPI scale
        float                                     lastThemeScale = -1.0f;

        std::vector<Slot>                              slots; // per-tab, matching tree traversal order
        std::vector<sky::ui::UIElement *>              splitterHandles;
        std::vector<SplitterBand>                      splitterBands;
        sky::ui::UIElement                            *dropHighlight = nullptr;
        sky::ui::UIElement                            *dragGhost     = nullptr;
        std::string                                    dragPanel;
        std::string                                    dragGhostText;
        std::string                                    pendingFloatPanel;
        bool                                           dragActive         = false;
        float                                          dragStartX         = 0.0f;
        float                                          dragStartY         = 0.0f;
        sky::ui::UIElement                            *menuBarElement     = nullptr;
        sky::ui::UIElement                            *statusBarElement   = nullptr;
        FileBrowserDialog                             *fileBrowserElement = nullptr;
        std::function<void(const FileBrowserResult &)> browserCallback;
        FileBrowserRequest                             browserRequest;
        bool                                           browserOpen  = false;
        float                                          width        = 1280.0f;
        float                                          height       = 720.0f;
        float                                          headerHeight = 24.0f;
        float                                          footerHeight = 22.0f;

        std::string statusProject = "SkyEngine";
        std::string statusRhi     = "-";
        std::string statusMode    = "Edit";
        float       statusFps     = 0.0f;

        bool built          = false;
        bool pendingRebuild = false;
        bool layoutDirty    = false;
    };

} // namespace sky::editor
