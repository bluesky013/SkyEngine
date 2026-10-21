#pragma once

#include <string>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {
    struct ShaderSourceFile {
        std::string path;
        std::string content;
        bool        operator==(const ShaderSourceFile &) const = default;
    };

    // Owns source bytes; no plugin callback or package handle survives registration.
    struct ShaderSourceBundle {
        std::string                   owner;
        std::vector<std::string>      dependencies;
        std::vector<ShaderSourceFile> files;
        std::vector<std::string>      includeRoots;
        bool                          operator==(const ShaderSourceBundle &) const = default;
    };

    bool IsShaderLogicalPath(const std::string &path);
    bool
    LoadShaderSourceBundles(sky::IFileSystem &root, const std::string &owner, std::vector<ShaderSourceBundle> &bundles, std::string *error = nullptr);
} // namespace sky::aurora
