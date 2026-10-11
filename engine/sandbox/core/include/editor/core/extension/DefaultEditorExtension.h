//
// Created on 2026/09/22.
//

#pragma once

#include <editor/core/extension/EditorExtension.h>
#include <editor/core/layout/PanelRegistry.h>
#include <string>
#include <vector>

namespace sky::editor {

    // Registers the default editor panels into a PanelRegistry. Routing the
    // built-in panels through an extension keeps panel registration a single
    // extension seam (external extensions add panels the same way).
    class DefaultEditorExtension : public EditorExtension {
    public:
        explicit DefaultEditorExtension(PanelRegistry &inRegistry) : registry(inRegistry)
        {
        }
        ~DefaultEditorExtension() override = default;

        const char *GetName() const override
        {
            return "DefaultEditorPanels";
        }

        void Register() override;
        void Unregister() override;

        // The built-in editor icon root ("icons/"), so the host scans it through the same
        // extension seam plugins use instead of a hard-coded name list.
        std::vector<std::string> GetIconDirectories() const override;

    private:
        PanelRegistry           &registry;
        std::vector<std::string> registeredIds;
    };

} // namespace sky::editor
