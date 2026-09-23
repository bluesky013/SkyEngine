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
        explicit DefaultEditorExtension(PanelRegistry &inRegistry) : registry(inRegistry) {}
        ~DefaultEditorExtension() override = default;

        const char *GetName() const override { return "DefaultEditorPanels"; }

        void Register() override;
        void Unregister() override;

    private:
        PanelRegistry           &registry;
        std::vector<std::string> registeredIds;
    };

} // namespace sky::editor
