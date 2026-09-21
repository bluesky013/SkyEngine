//
// Created on 2026/09/21.
//
// Non-Qt experimental sandbox for the editor redesign. It drives the editor
// shell (native window + frame loop + EditorCore) and is a leaf target: no
// shipping module depends on it.
//

#include "EditorShell.h"
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Sandbox";

using namespace sky;

int main()
{
    if (!Platform::Get()->Init({})) {
        LOG_E(TAG, "Platform init failed");
        return -1;
    }

    // Clipboard round-trip through the platform shell service.
    if (char *clipboard = Platform::Get()->GetClipBoardText(); clipboard != nullptr) {
        LOG_I(TAG, "clipboard read: %s", clipboard);
        Platform::Get()->FreeClipBoardText(clipboard);
    }

    sky::editor::sandbox::EditorShell shell;
    if (!shell.Init()) {
        return -1;
    }

    // Phase 1 validation runs for a fixed duration; a real shell would pass 0
    // to run until the window closes.
    shell.Run(5.f);
    shell.Shutdown();
    return 0;
}
