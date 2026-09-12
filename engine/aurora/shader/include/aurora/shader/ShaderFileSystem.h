//
// ShaderFileSystem: virtual include provider for shader compilation.
// Inherits core MultiFileSystem for search-path priority across mounted
// IFileSystem instances, and adds in-memory virtual files (packaged content)
// plus a Slang ISlangFileSystem bridge.
//

#pragma once

#include <core/file/MultiFileSystem.h>

#include <string>
#include <unordered_map>

struct ISlangFileSystem;

namespace sky::aurora {

    class ShaderFileSystem : public sky::MultiFileSystem {
    public:
        ShaderFileSystem() = default;
        ~ShaderFileSystem() override;

        ShaderFileSystem(const ShaderFileSystem &) = delete;
        ShaderFileSystem &operator=(const ShaderFileSystem &) = delete;

        // Convenience: mount an on-disk search directory.
        void AddSearchPath(std::string directory);

        // Register in-memory content at a logical path (top priority; stands in
        // for a packaged shader source).
        void AddVirtualFile(std::string path, std::string content);

        // Look up an in-memory virtual file; nullptr when not registered.
        const std::string *Find(const std::string &path) const;

        // Slang file system adapter (global ::ISlangFileSystem) that resolves
        // #include against virtual files (top priority) then mounted file systems.
        ISlangFileSystem *GetSlangFileSystem();

    private:
        bool Resolve(const std::string &path, std::string &content);

        std::unordered_map<std::string, std::string> mFiles;
        class SlangAdapter;
        SlangAdapter *mAdapter = nullptr;
    };

} // namespace sky::aurora
