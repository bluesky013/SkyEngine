//
// Created on 2026/09/21.
//

#pragma once

#include <editor/render/EditorRenderer.h>
#include <framework/interface/IModule.h>

namespace sky::editor {

    // Editor module: a thin adapter over the engine application/module system.
    // It owns an `EditorRenderer` (which hosts the Aurora frame) and delegates
    // the module lifecycle to it. No render details live here.
    class SandboxModule : public sky::IModule {
    public:
        SandboxModule();
        ~SandboxModule() override;

        bool Init(const sky::StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;

    private:
        EditorRenderer renderer;
        bool           initialized = false;
    };

} // namespace sky::editor
