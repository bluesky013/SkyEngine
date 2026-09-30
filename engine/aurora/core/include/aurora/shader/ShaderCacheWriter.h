//
// ShaderCacheWriter: a dedicated cache-IO thread that owns the in-memory local
// index and serializes all disk writes. Producers (worker/render threads) call
// Submit(); readers use the lock-free-ish snapshots for lookups. No cache write
// ever happens on the caller thread.
//

#pragma once

#include <aurora/shader/ShaderCacheStore.h>
#include <aurora/shader/ShaderCompile.h>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {

    class ShaderCacheWriter {
    public:
        static constexpr uint32_t kTargetCount = 3; // SPIRV / MSL / DXIL

        explicit ShaderCacheWriter(sky::IFileSystem *localRoot);
        ~ShaderCacheWriter();

        ShaderCacheWriter(const ShaderCacheWriter &)            = delete;
        ShaderCacheWriter &operator=(const ShaderCacheWriter &) = delete;

        void Start();
        void Stop();

        struct Pending {
            ShaderCacheKey           key;
            ShaderCompileResult      result;
            ShaderVariantSchema      schema;
            uint64_t                 schemaFp   = 0;
            uint64_t                 sourceHash = 0;
            std::string              relativePath;
            std::vector<std::string> deps;
        };

        // Enqueue from any thread (non-blocking).
        void Submit(Pending pending);

        // Block until the queue is drained and the index is persisted.
        void Flush();

        // ---- reader side (local root snapshot) ----
        bool LookupArtifact(uint32_t             target,
                            const std::string   &path,
                            uint64_t             schemaFp,
                            uint64_t             variantHash,
                            uint32_t             stage,
                            uint64_t             entryHash,
                            ShaderCacheArtifact &out) const;
        bool LookupSchema(uint32_t target, const std::string &path, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp) const;

    private:
        void ThreadMain();
        void ApplyBatch(const std::vector<Pending> &batch);
        void Publish(uint32_t target);

        sky::IFileSystem *mLocal = nullptr;

        std::thread             mThread;
        mutable std::mutex      mMutex;
        std::condition_variable mCv;
        std::deque<Pending>     mQueue;
        bool                    mRunning        = false;
        bool                    mFlushRequested = false;

        ShaderCacheIndex                        mLocalIndex[kTargetCount];
        std::shared_ptr<const ShaderCacheIndex> mSnapshot[kTargetCount];
    };

} // namespace sky::aurora
