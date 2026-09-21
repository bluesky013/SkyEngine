//
// Created on 2026/09/21.
//
// Module entry point for EditorCore. The editor services (undo/redo, property
// model, documents, selection) live under this module. It must stay free of
// Aurora, the legacy render stack, the UI layer, and Qt (enforced by a
// configure-time guard in CMakeLists.txt).
//

#include <editor/core/EditorCore.h>
#include <core/logger/Logger.h>

static const char *TAG = "EditorCore";

namespace sky::editor {

    CommandService &EditorCore::GetCommandService()
    {
        static CommandService service;
        return service;
    }

    void EditorCore::Init()
    {
        LOG_I(TAG, "EditorCore init");
    }

    void EditorCore::Shutdown()
    {
        EditorCore::GetCommandService().Clear();
        LOG_I(TAG, "EditorCore shutdown");
    }

} // namespace sky::editor
