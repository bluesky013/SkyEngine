//
// Created on 2026/09/21.
//
// Skeleton for the editor-owned renderer. The GUI pipeline (UI shaders, PSOs,
// batching) and the UI render-graph pass land here (tasks 8.1/8.2). For now it
// validates the module scaffolding without touching the RHI.
//

#include <editor/render/EditorRenderer.h>
#include <core/logger/Logger.h>

static const char *TAG = "EditorRender";

namespace sky::editor {

    bool EditorRenderer::Init(void *windowHandle, uint32_t inWidth, uint32_t inHeight)
    {
        (void)windowHandle;
        width = inWidth;
        height = inHeight;
        initialized = true;
        LOG_I(TAG, "EditorRenderer init placeholder (%ux%u): GUI pipeline pending", width, height);
        return true;
    }

    void EditorRenderer::Shutdown()
    {
        initialized = false;
    }

    void EditorRenderer::RenderFrame(float deltaTime)
    {
        (void)deltaTime; // GUI pipeline pending.
    }

} // namespace sky::editor
