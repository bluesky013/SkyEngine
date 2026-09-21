//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/extension/EditorExtension.h>
#include <cstddef>
#include <string>
#include <vector>

namespace sky::editor {

    // Owns editor extensions and drives their registration.
    //
    // Extensions can be added in-process (tests, statically linked) or, later,
    // instantiated from loaded modules. The host has no toolkit/render
    // dependency.
    class EditorExtensionHost {
    public:
        EditorExtensionHost() = default;
        ~EditorExtensionHost();

        EditorExtensionHost(const EditorExtensionHost &) = delete;
        EditorExtensionHost &operator=(const EditorExtensionHost &) = delete;

        void Add(EditorExtensionPtr extension);
        void RegisterAll();
        void UnregisterAll();

        bool IsRegistered() const { return registered; }
        size_t GetCount() const { return extensions.size(); }
        const EditorExtension *Find(const std::string &name) const;

    private:
        std::vector<EditorExtensionPtr> extensions;
        bool registered = false;
    };

} // namespace sky::editor
