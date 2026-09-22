//
// Created on 2026/09/22.
//

#pragma once

#include <string>

namespace sky::editor {

    // Sandbox-owned editor resources.
    //
    // The editor ships its own builtin resources (icons, editor fonts, ...) under
    // `engine/sandbox/resources/`, deployed next to the executable as `resources/`.
    // This is distinct from the project package assets under the repo-root
    // `assets/`, which are being retired: new editor builtin resources belong here.
    class SandboxResources {
    public:
        // Absolute path to the deployed editor resources root (with trailing '/').
        static std::string Root();

        // Absolute path to `relative` under the editor resources root.
        static std::string Resolve(const std::string &relative);
    };

} // namespace sky::editor
