//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/CommandService.h>

namespace sky::editor {

    // Toolkit- and render-independent editor core.
    // Hosts the editor services (undo/redo, the reflection property model,
    // documents, selection); UI panels are views over these services.
    class EditorCore {
    public:
        static void Init();
        static void Shutdown();

        // The editor-wide undo/redo service.
        static CommandService &GetCommandService();
    };

} // namespace sky::editor
