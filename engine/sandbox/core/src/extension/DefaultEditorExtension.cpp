//
// Created on 2026/09/22.
//

#include <editor/core/extension/DefaultEditorExtension.h>
#include <editor/core/layout/DefaultPanels.h>

namespace sky::editor {

    void DefaultEditorExtension::Register()
    {
        if (!registeredIds.empty()) {
            return;
        }
        RegisterDefaultEditorPanels(registry);
        // The ids registered by RegisterDefaultEditorPanels (kept here so the
        // extension can unregister them again).
        for (const char *id : {"viewport", "outliner", "inspector", "outputlog", "console"}) {
            registeredIds.emplace_back(id);
        }
    }

    void DefaultEditorExtension::Unregister()
    {
        for (const auto &id : registeredIds) {
            registry.Unregister(id);
        }
        registeredIds.clear();
    }

} // namespace sky::editor
