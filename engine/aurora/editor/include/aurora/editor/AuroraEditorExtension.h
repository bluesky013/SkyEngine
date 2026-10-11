//
// Aurora editor extension: the aurora editor module stages this into EditorExtensionRegistry;
// the editor host drains it and calls Register() to wire the aurora per-asset views, and reads
// GetIconDirectories() to pick up the plugin's icon root. See docs/editor/ui-standards.md.
//

#pragma once

#include <editor/core/extension/EditorExtension.h>

#include <string>
#include <vector>

namespace sky::aurora::editor {

    class AuroraEditorExtension : public sky::editor::EditorExtension {
    public:
        const char *GetName() const override;
        void        Register() override;
        void        Unregister() override;

        std::vector<std::string> GetIconDirectories() const override;
    };

} // namespace sky::aurora::editor
