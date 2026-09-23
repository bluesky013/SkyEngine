//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/extension/EditorExtensionHost.h>
#include <editor/render/EditorRenderer.h>
#include <editor/shell/EditorShell.h>
#include <framework/interface/IModule.h>
#include <framework/window/IWindowEvent.h>

namespace sky::editor {

    // Editor module: a thin adapter over the engine application/module system.
    // It owns an `EditorRenderer` (which hosts the Aurora frame) and delegates
    // the module lifecycle to it. No render details live here.
    class SandboxModule : public sky::IModule, public sky::IMouseEvent, public sky::IKeyboardEvent {
    public:
        SandboxModule();
        ~SandboxModule() override;

        bool Init(const sky::StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;

        // Platform input (framework broadcasts these); forwarded to the shell.
        void OnMouseButtonDown(const sky::MouseButtonEvent &event) override;
        void OnMouseButtonUp(const sky::MouseButtonEvent &event) override;
        void OnMouseMotion(const sky::MouseMotionEvent &event) override;
        void OnMouseWheel(const sky::MouseWheelEvent &event) override;
        void OnKeyUp(const sky::KeyboardEvent &event) override;
        void OnKeyDown(const sky::KeyboardEvent &event) override;
        void OnTextInput(sky::WindowID winID, const char *text) override;

    private:
        EditorRenderer renderer;

        // EditorCore services (render-independent) owned by the host.
        PanelRegistry        panelRegistry;
        LayoutModel          layoutModel;
        SelectionService     selection;
        LogService           logService;
        CommandController    commandController;
        EditorExtensionHost  extensionHost;

        // UI-linked shell that composes the panels from the services above.
        EditorShell shell;

        sky::EventBinder<sky::IMouseEvent>    mouseBinder;
        sky::EventBinder<sky::IKeyboardEvent> keyBinder;

        bool initialized = false;
    };

} // namespace sky::editor
