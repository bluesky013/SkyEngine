//
// EditorExtensionRegistry: cross-DLL staging for plugin editor extensions.
//
// Plugin editor modules (e.g. Aurora.Render.Editor / Content.Views) add their EditorExtension here
// during Init; the editor host (SandboxModule) drains it into its EditorExtensionHost before
// RegisterAll, so loaded modules contribute asset/actor creators, actions, etc. without linking the
// host. See docs/editor/ui-standards.md.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/core/extension/EditorExtension.h>

#include <utility>
#include <vector>

namespace sky::editor {

    class EditorExtensionRegistry : public sky::Singleton<EditorExtensionRegistry> {
        friend class sky::Singleton<EditorExtensionRegistry>;

    public:
        void Add(EditorExtensionPtr extension)
        {
            pending.push_back(std::move(extension));
        }

        // Transfers ownership of all staged extensions to the caller (the host).
        std::vector<EditorExtensionPtr> TakeAll()
        {
            return std::move(pending);
        }

    private:
        std::vector<EditorExtensionPtr> pending;
    };

} // namespace sky::editor
