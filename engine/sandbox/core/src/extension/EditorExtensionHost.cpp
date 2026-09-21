//
// Created on 2026/09/21.
//

#include <editor/core/extension/EditorExtensionHost.h>

namespace sky::editor {

    EditorExtensionHost::~EditorExtensionHost()
    {
        UnregisterAll();
    }

    void EditorExtensionHost::Add(EditorExtensionPtr extension)
    {
        if (extension == nullptr) {
            return;
        }
        if (registered) {
            extension->Register();
        }
        extensions.push_back(std::move(extension));
    }

    void EditorExtensionHost::RegisterAll()
    {
        if (registered) {
            return;
        }
        for (auto &extension : extensions) {
            extension->Register();
        }
        registered = true;
    }

    void EditorExtensionHost::UnregisterAll()
    {
        if (!registered) {
            return;
        }
        for (auto it = extensions.rbegin(); it != extensions.rend(); ++it) {
            (*it)->Unregister();
        }
        registered = false;
    }

    const EditorExtension *EditorExtensionHost::Find(const std::string &name) const
    {
        for (const auto &extension : extensions) {
            if (name == extension->GetName()) {
                return extension.get();
            }
        }
        return nullptr;
    }

} // namespace sky::editor
