//
// Created on 2026/09/21.
//

#pragma once

#include <cstddef>
#include <editor/core/extension/EditorExtension.h>
#include <string>
#include <vector>

namespace sky::editor {

    // Owns extension instances and their session registration without render dependencies.
    class EditorExtensionHost {
    public:
        EditorExtensionHost() = default;
        ~EditorExtensionHost();

        EditorExtensionHost(const EditorExtensionHost &)            = delete;
        EditorExtensionHost &operator=(const EditorExtensionHost &) = delete;

        void             Add(EditorExtensionPtr extension);
        void             Add(EditorExtensionRegistration extension);
        void             RegisterAll();
        void             UnregisterAll();
        EditorEditResult MakePropertyEdit(Actor &, const PropertyDescriptor &, const Any &) const;
        EditorEditResult PrepareComponentAddition(Actor &, ComponentBase &) const;

        bool IsRegistered() const
        {
            return registered;
        }
        size_t GetCount() const
        {
            return extensions.size();
        }
        const EditorExtension *Find(const std::string &name) const;

        // The owned extensions, e.g. so the host can collect their contributed icon roots.
        const std::vector<EditorExtensionRegistration> &GetExtensions() const
        {
            return extensions;
        }

    private:
        std::vector<EditorExtensionRegistration> extensions;
        bool                                     registered = false;
    };

} // namespace sky::editor
