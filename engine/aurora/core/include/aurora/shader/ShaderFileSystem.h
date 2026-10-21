#pragma once

#include <aurora/shader/ShaderSourceBundle.h>
#include <core/file/MultiFileSystem.h>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace sky::aurora {

    class ShaderFileSystem : public sky::MultiFileSystem {
    public:
        // Register/snapshot on the owner thread; workers only read their independent snapshots.
        ShaderFileSystem() = default;
        ~ShaderFileSystem() override;

        ShaderFileSystem(const ShaderFileSystem &)            = delete;
        ShaderFileSystem &operator=(const ShaderFileSystem &) = delete;

        // Convenience: mount an on-disk search directory.
        void AddSearchPath(std::string directory);

        // Unowned legacy inputs cannot replace registered bundle paths.
        bool AddVirtualFile(std::string path, std::string content);

        // Registration is transactional; registered bundles cannot be overwritten by legacy virtual files.
        bool                              AddBundles(std::span<const ShaderSourceBundle> incoming, std::string *error = nullptr);
        bool                              RemoveBundle(const std::string &owner, std::string *error = nullptr);
        bool                              LoadBundles(sky::IFileSystem &root, const std::string &owner, std::string *error = nullptr);
        std::shared_ptr<ShaderFileSystem> Snapshot(const std::string &rootPath) const;
        uint64_t                          BundleFingerprint() const;
        std::vector<std::string>          IncludeRoots() const;
        bool                              IsBundlePath(const std::string &path) const;

        // Read an exact canonical path; bundle contents precede unowned loose sources.
        bool ReadFile(const std::string &path, std::string &content);

    private:
        bool Resolve(const std::string &path, std::string &content);

        std::unordered_map<std::string, std::string>                               mFiles;
        std::unordered_map<std::string, std::shared_ptr<const ShaderSourceBundle>> bundles;
    };

} // namespace sky::aurora
