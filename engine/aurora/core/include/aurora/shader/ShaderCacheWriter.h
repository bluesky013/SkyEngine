#pragma once

#include <aurora/shader/ShaderCacheStore.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace sky::aurora {
    class ShaderCacheWriter {
    public:
        static constexpr uint32_t kTargetCount = 3;
        explicit ShaderCacheWriter(sky::IFileSystem *localRoot);
        ~ShaderCacheWriter();
        ShaderCacheWriter(const ShaderCacheWriter &)            = delete;
        ShaderCacheWriter &operator=(const ShaderCacheWriter &) = delete;
        bool               Start(std::string *error = nullptr);
        bool               Stop(std::string *error = nullptr);
        sky::IFileSystem  *GetRoot() const
        {
            return mLocal;
        }
        struct Pending {
            ShaderCacheKey           key;
            ShaderCompileResult      result;
            ShaderVariantSchema      schema;
            uint64_t                 schemaFp   = 0;
            uint64_t                 sourceHash = 0;
            std::string              relativePath;
            std::vector<std::string> deps;
        };
        bool Submit(Pending pending, std::string *error = nullptr);
        // Covers every submission accepted before this call; reports a latched persistence failure.
        bool Flush(std::string *error = nullptr);
        bool LookupArtifact(const ShaderCacheKey &key, ShaderCacheArtifact &out) const;
        bool LookupSchema(uint32_t target, const std::string &path, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp) const;

    private:
        void                                    ThreadMain();
        bool                                    Apply(const Pending &pending, std::string &error);
        void                                    Publish(uint32_t target, ShaderCacheIndex index);
        bool                                    Status(std::string *error) const;
        sky::IFileSystem                       *mLocal = nullptr;
        std::thread                             mThread;
        mutable std::mutex                      mMutex;
        std::condition_variable                 mCv;
        std::deque<Pending>                     mQueue;
        bool                                    mRunning   = false;
        uint64_t                                mSubmitted = 0;
        uint64_t                                mCompleted = 0;
        std::string                             mError;
        std::shared_ptr<const ShaderCacheIndex> mSnapshot[kTargetCount];
    };
} // namespace sky::aurora
