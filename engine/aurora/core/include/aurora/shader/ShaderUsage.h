// Target-specific shader usage and offline product verification.

#pragma once

#include <aurora/shader/ShaderVariant.h>
#include <core/environment/Singleton.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {

    class ShaderResolver;

    struct ShaderUsageEntry {
        std::string             relativePath;
        std::string             entry;
        uint32_t                stage         = 0;
        uint32_t                target        = 0;
        uint64_t                variantHash   = 0;
        uint64_t                variantKey[2] = {0, 0};
        std::string             variantDump; // human-readable key values
        uint64_t                toolchainFp = 0;
        ShaderVariant           variant;
        ShaderTargetEnvironment environment;
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

        // Dedup compares full environment and variant values, including specialization.
        bool Record(const ShaderUsageEntry &entry, std::string *error = nullptr);

        size_t Size() const
        {
            return mEntries.size();
        }
        const std::vector<ShaderUsageEntry> &Entries() const
        {
            return mEntries;
        }

        // Union-merge the in-memory set with the JSON already at `path`.
        bool Flush(sky::IFileSystem &fs, const std::string &path, std::string *error = nullptr);

        static constexpr uint32_t kFormatVersion = 3;
        static bool Load(sky::IFileSystem &fs, const std::string &path, std::vector<ShaderUsageEntry> &out, std::string *error = nullptr);

    private:
        bool                          mEnabled = false;
        std::vector<ShaderUsageEntry> mEntries;
    };

    // Process-wide runtime collector. Core is a static library linked into every
    // module DLL, so the instance must live in Environment rather than a
    // function-local static.
    class ShaderUsageStore : public Singleton<ShaderUsageStore> {
    public:
        static bool Enabled();
        static void SetEnabled(bool enabled);
        static void Record(const ShaderUsageEntry &entry);
        static size_t Size();
        static std::vector<ShaderUsageEntry> Snapshot();
        static void Clear();
        static bool Flush(sky::IFileSystem &fs, const std::string &path);
        static bool Flush(const std::string &file);

    private:
        friend class Singleton<ShaderUsageStore>;
        ShaderUsageStore() = default;

        std::mutex            mMutex;
        ShaderUsageCollector mCollector;
    };

    // Group by path, target environment and full variant values.
    struct ShaderUsageGroup {
        std::string                   relativePath;
        uint32_t                      target      = 0;
        uint64_t                      variantHash = 0;
        ShaderTargetEnvironment       environment;
        std::vector<ShaderUsageEntry> entries; // per-entry (dedup by entry/stage)
    };
    std::vector<ShaderUsageGroup> GroupShaderUsage(const std::vector<ShaderUsageEntry> &usage);

    // Offline precompile: resolve + compile each usage group into the resolver's
    // cache (same code path as runtime, no GPU).
    // Offline precompile: resolve + compile each usage group into the resolver's
    // cache. `pipelineSchema` declares the external P-domain keys; `variant` may
    // combine them with material keys.
    bool BuildUsageCache(ShaderResolver                   &resolver,
                         const std::vector<ShaderUsageEntry> &usage,
                         std::string                       *error          = nullptr,
                         const ShaderVariantSchema         *pipelineSchema = nullptr);

} // namespace sky::aurora
