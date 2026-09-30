//
// ShaderUsage: runtime shader-use collection for offline precompilation.
//
// The resolver records every resolved (path, entry, stage, target, variant) so
// a host tool can precompile exactly the used set into the shipped cache.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {

    class ShaderResolver;

    struct ShaderUsageEntry {
        std::string relativePath;
        std::string entry;
        uint32_t    stage         = 0;
        uint32_t    target        = 0;
        uint64_t    variantHash   = 0;
        uint64_t    variantKey[2] = {0, 0};
        std::string variantDump; // human-readable key values
        uint64_t    toolchainFp = 0;
    };

    // In-memory dedup set; `Flush` union-merges into a JSON file.
    class ShaderUsageCollector {
    public:
        bool Enabled() const
        {
            return mEnabled;
        }
        void SetEnabled(bool enabled)
        {
            mEnabled = enabled;
        }

        // Dedup by (relativePath, entry, target, variantHash).
        void Record(const ShaderUsageEntry &entry);

        size_t Size() const
        {
            return mEntries.size();
        }
        const std::vector<ShaderUsageEntry> &Entries() const
        {
            return mEntries;
        }

        // Union-merge the in-memory set with the JSON already at `path`.
        bool Flush(sky::IFileSystem &fs, const std::string &path);

        static bool Load(sky::IFileSystem &fs, const std::string &path, std::vector<ShaderUsageEntry> &out);

    private:
        bool                          mEnabled = false;
        std::vector<ShaderUsageEntry> mEntries;
    };

    // Group usage by (relativePath, target, variantHash) -> entry union.
    struct ShaderUsageGroup {
        std::string                   relativePath;
        uint32_t                      target      = 0;
        uint64_t                      variantHash = 0;
        std::vector<ShaderUsageEntry> entries; // per-entry (dedup by entry/stage)
    };
    std::vector<ShaderUsageGroup> GroupShaderUsage(const std::vector<ShaderUsageEntry> &usage);

    // Offline precompile: resolve + compile each usage group into the resolver's
    // cache (same code path as runtime, no GPU).
    bool BuildUsageCache(ShaderResolver &resolver, const std::vector<ShaderUsageEntry> &usage, std::string *error = nullptr);

} // namespace sky::aurora
