//
// Created on 2026/09/21.
//
// Thin editor module: delegates to the editor-owned renderer (EditorRender).
//

#include <editor/sandbox/SandboxModule.h>

static const char *TAG = "SandboxModule";

namespace sky::editor {

    SandboxModule::SandboxModule() = default;
    SandboxModule::~SandboxModule() = default;

    bool SandboxModule::Init(const sky::StartArguments & /*args*/)
    {
        initialized = renderer.Init("SandboxEditor", 1280, 720);
        return initialized;
    }

    void SandboxModule::Start()
    {
        if (initialized) {
            renderer.Start();
        }
    }

    void SandboxModule::Tick(float delta)
    {
        if (initialized) {
            renderer.Tick(delta);
        }
    }

    void SandboxModule::Shutdown()
    {
        if (initialized) {
            renderer.Shutdown();
        }
        initialized = false;
    }

} // namespace sky::editor
