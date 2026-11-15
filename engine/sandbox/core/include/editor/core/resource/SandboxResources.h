//
// Created on 2026/09/22.
//

#pragma once

#include <string>

namespace sky::editor {

    // Packaged editor chrome (icons/fonts); scene assets belong to the Engine asset mount.
    class SandboxResources {
    public:
        // Absolute path to the deployed editor resources root (with trailing '/').
        static std::string Root();

        // Absolute path to `relative` under the editor resources root.
        static std::string Resolve(const std::string &relative);

        // Writable per-user cache root; empty disables persistence.
        static std::string CacheRoot();
    };

} // namespace sky::editor
