//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/document/WorldDocument.h>
#include <editor/core/extension/EditorExtensionHost.h>
#include <editor/core/play/PlaySession.h>
#include <editor/core/preferences/PreferenceRegistry.h>
#include <editor/core/preferences/PreferenceStore.h>
#include <editor/render/EditorRenderer.h>
#include <editor/sandbox/UiTarget.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/FileBrowserDialog.h>
#include <editor/shell/ProjectManagerView.h>
#include <framework/interface/IModule.h>
#include <framework/project/ProjectDescriptor.h>
#include <framework/project/ProjectLock.h>
#include <framework/window/IWindowEvent.h>
#include <ui/UIContext.h>

#include <memory>

namespace sky::editor {

    // Editor module: a thin adapter over the engine application/module system.
    //
    // Two modes decided by `--project`:
    //   * no project  -> the Project Manager (hub) view;
    //   * --project X -> the editor shell bound to project X.
    class SandboxModule : public sky::IModule, public sky::IMouseEvent, public sky::IKeyboardEvent {
    public:
        SandboxModule();
        ~SandboxModule() override;

        bool Init(const sky::StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;

        // Platform input (framework broadcasts these); forwarded to the shell/hub.
        void OnMouseButtonDown(const sky::MouseButtonEvent &event) override;
        void OnMouseButtonUp(const sky::MouseButtonEvent &event) override;
        void OnMouseMotion(const sky::MouseMotionEvent &event) override;
        void OnMouseWheel(const sky::MouseWheelEvent &event) override;
        void OnKeyUp(const sky::KeyboardEvent &event) override;
        void OnKeyDown(const sky::KeyboardEvent &event) override;
        void OnTextInput(sky::WindowID winID, const char *text) override;

    private:
        bool BuildHub();
        bool BuildEditor();
        void OpenProject(const std::string &skyprojPath);
        void AddProject();
        void NewProject();
        void NewWorld();
        void OpenWorld();
        void SaveWorld();
        void CloseWorld();
        void Quit();
        void StopPlay();
        void RefreshDocumentInfo();
        // Directory new/open world dialogs start in (project assets, else user config).
        std::string WorldBasePath() const;
        // Effective UI scale = system DPI scale * the `editor.uiScale` preference.
        float       EffectiveUiScale() const;
        void        OnFileBrowserResult(const FileBrowserResult &result);
        void        CreateProjectFromResult(const FileBrowserResult &result);
        void        AddProjectFromResult(const FileBrowserResult &result);
        void        RegisterPreferencePages();
        void        LoadPreferences();
        void        SavePreferences();
        std::string PreferencePath() const;
        void        RemoveFromList(const std::string &skyprojPath);
        void        DeleteProjectFolder(const std::string &skyprojPath);
        void        RefreshHubRecent();
        bool        ValidateProject(const ProjectDescriptor &descriptor, std::string &message);
        void        PaintGui(sky::ui::UIPaintContext &context, uint32_t width, uint32_t height);

        // Per-window input routing: records the primary (main) window id on first
        // event and accepts only events for it (or id-less ones as a fallback).
        bool AcceptWindowEvent(sky::WindowID winID);

        // Active UI input sink (hub view or editor shell) for the current mode.
        IUiTarget *ActiveTarget() const
        {
            return (hubMode ? hubTarget : shellTarget).get();
        }

        // Routes a pointer event to the active target (main/hub) or, for a
        // floating window, to that surface.
        void RoutePointer(const sky::ui::UIPointerEvent &pointer, sky::WindowID winID);
        bool RouteFloatingPointer(sky::WindowID winID, const sky::ui::UIPointerEvent &pointer);

        EditorRenderer renderer;

        // EditorCore services (render-independent) owned by the host.
        PanelRegistry       panelRegistry;
        LayoutModel         layoutModel;
        SelectionService    selection;
        LogService          logService;
        CommandController   commandController;
        EditorExtensionHost extensionHost;

        PreferenceRegistry               preferenceRegistry;
        std::unique_ptr<PreferenceStore> preferenceStore;
        std::unique_ptr<WorldDocument>   worldDocument;
        PlaySession                      playSession;
        WorldDocument                   *lastDocPtr     = nullptr; // detects document switches
        bool                             lastDocDirty   = false;
        bool                             docInfoApplied = false; // forces the first title/status push

        // UI-linked shell that composes the panels from the services above.
        EditorShell shell;

        // UI input targets (one active per mode).
        std::unique_ptr<IUiTarget> hubTarget;
        std::unique_ptr<IUiTarget> shellTarget;

        // Hub (project manager) mode.
        bool                                hubMode = false;
        std::unique_ptr<sky::ui::UIContext> hubContext;
        ProjectManagerView                 *hubView = nullptr;
        FileBrowserDialog                  *browser = nullptr;
        ProjectDescriptor                   project;
        sky::ProjectLock                    projectLock;

        sky::EventBinder<sky::IMouseEvent>    mouseBinder;
        sky::EventBinder<sky::IKeyboardEvent> keyBinder;

        bool          initialized   = false;
        float         systemUiScale = 1.0f;     // DPI scale (dpi/96); SKY_UI_SCALE overrides
        float         uiScale       = 1.0f;     // effective = systemUiScale * editor.uiScale
        std::string   layoutPath;               // per-user editor layout file (empty if unavailable)
        std::string   rhiName         = "Auto"; // active RHI name for the status bar
        sky::WindowID primaryWindowId = 0;      // main window id, learned from the first event
    };

} // namespace sky::editor
