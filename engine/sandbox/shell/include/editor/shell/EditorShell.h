//
// Created on 2026/09/22.
//

#pragma once

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/console/CommandController.h>
#include <editor/core/log/LogService.h>
#include <editor/core/property/PropertyModel.h>
#include <editor/core/selection/SelectionService.h>
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

        EditorShell(const EditorShell &) = delete;
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

        // Registers a view factory for a panel id. Falls back to a titled frame
        // when no factory is registered.
        void RegisterPanelView(const std::string &panelId, PanelViewFactory factory);

        // Registers the built-in panel views for the default panel ids.
        void RegisterBuiltinPanelViews();

        // (Re)builds the element tree from the current layout + registry.
        void Rebuild();
        bool IsBuilt() const { return built; }

        // Sets the surface size and re-applies panel rectangles.
        void Layout(float width, float height);
        void Paint(sky::ui::UIPaintContext &context);

        // Input entry points (device pixels). The host maps platform events to
        // these and forwards them; the shell dispatches through the UI event
        // router. Returns whether the event was handled.
        bool DispatchPointer(const sky::ui::UIPointerEvent &event);
        bool DispatchKey(const sky::ui::UIKeyEvent &event);
        bool DispatchText(const sky::ui::UITextInputEvent &event);
        sky::ui::UIElement *HitTest(float x, float y) const;

        // Whether the UI wants input (used to gate viewport input).
        bool WantsInput() const;

        size_t GetPanelCount() const { return panels.size(); }
        bool HasPanel(const std::string &panelId) const;
        // Toggles a panel's visibility through the layout model and rebuilds.
        void SetPanelVisible(const std::string &panelId, bool visible);

    private:
        // One layout tab: an optional header row plus the active panel's body.
        struct Slot {
            sky::ui::UIElement *header = nullptr;
            sky::ui::UIElement *body   = nullptr;
        };

        void ApplyTheme();
        void CreateNode(LayoutNode *node, const sky::ui::UIRect &rect);
        void ApplyNode(LayoutNode *node, const sky::ui::UIRect &rect, size_t &panelCursor);
        sky::ui::UIElement *CreatePanelView(const std::string &panelId);

        std::unique_ptr<sky::ui::UIContext>      context;
        std::unique_ptr<sky::ui::UIEventRouter> eventRouter;
        LayoutModel *layoutModel = nullptr;
        PanelRegistry *panelRegistry = nullptr;
        sky::ui::UITextSystem *textSystem = nullptr;
        SelectionService *selection = nullptr;
        LogService *logService = nullptr;
        CommandController *commandController = nullptr;
        PropertyModel *inspectorModel = nullptr;

        std::unordered_map<std::string, PanelViewFactory> viewFactories;
        std::vector<sky::ui::UIElement *> panels; // panel bodies, creation/traversal order
        std::vector<Slot>                 slots;  // per-tab, matching tree traversal order
        sky::ui::UIElement               *toolbarElement = nullptr;
        float width = 1280.0f;
        float height = 720.0f;
        float toolbarHeight = 26.0f;
        bool built = false;
        bool pendingRebuild = false;
    };

} // namespace sky::editor
