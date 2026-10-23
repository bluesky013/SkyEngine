#include <editor/render/EditorRenderer.h>

#include "EditorFramePacer.h"
#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <editor/render/UiSnapshotProducer.h>
#include <framework/window/NativeWindow.h>

namespace sky::editor {
    EditorRenderer::EditorRenderer() : framePacer(std::make_unique<EditorFramePacer>())
    {
    }

    EditorRenderer::~EditorRenderer()
    {
        Shutdown();
    }
} // namespace sky::editor
