#include <aurora/shader/ShaderCacheWriter.h>

#include <core/file/FileSystem.h>

namespace sky::aurora {
    ShaderCacheWriter::ShaderCacheWriter(sky::IFileSystem *localRoot) : mLocal(localRoot)
    {
    }
    ShaderCacheWriter::~ShaderCacheWriter()
    {
        Stop();
    }
    bool ShaderCacheWriter::Status(std::string *error) const
    {
        if (error != nullptr) {
            *error = mError;
        }
        return mError.empty();
    }
    bool ShaderCacheWriter::Start(std::string *error)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mRunning) {
            return Status(error);
        }
        if (mLocal == nullptr || mThread.joinable()) {
            mError = "shader writer has no root or is stopping";
            return Status(error);
        }
        mError.clear();
        mSubmitted = mCompleted = 0;
        for (uint32_t target = 0; target < kTargetCount; ++target) {
            ShaderCacheIndex index;
            const auto       directory = std::string(ShaderTargetDirName(target));
            if (mLocal->FileExist(sky::FilePath(directory + "/index.bin")) && !LoadIndex(*mLocal, directory, index)) {
                mError = "shader writer index is corrupt or requires recook: " + directory;
                return Status(error);
            }
            mSnapshot[target] = std::make_shared<const ShaderCacheIndex>(std::move(index));
        }
        mRunning = true;
        mThread  = std::thread([this] { ThreadMain(); });
        return true;
    }
    bool ShaderCacheWriter::Stop(std::string *error)
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRunning = false;
        }
        mCv.notify_all();
        if (mThread.joinable()) {
            mThread.join();
        }
        std::lock_guard<std::mutex> lock(mMutex);
        return Status(error);
    }
    bool ShaderCacheWriter::Submit(Pending pending, std::string *error)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning || !mError.empty()) {
            if (error != nullptr) {
                *error = mError.empty() ? "shader writer is stopped" : mError;
            }
            return false;
        }
        if (pending.key.target >= kTargetCount) {
            if (error != nullptr) {
                *error = "shader writer target is invalid";
            }
            return false;
        }
        mQueue.push_back(std::move(pending));
        ++mSubmitted;
        mCv.notify_one();
        return true;
    }
    bool ShaderCacheWriter::Flush(std::string *error)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        const auto                   submitted = mSubmitted;
        mCv.wait(lock, [this, submitted] { return mCompleted >= submitted; });
        return Status(error);
    }
    void ShaderCacheWriter::ThreadMain()
    {
        for (;;) {
            Pending pending;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCv.wait(lock, [this] { return !mRunning || !mQueue.empty(); });
                if (mQueue.empty()) {
                    break;
                }
                pending = std::move(mQueue.front());
                mQueue.pop_front();
            }
            std::string error;
            const bool  succeeded = Apply(pending, error);
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!succeeded && mError.empty()) {
                    mError = error;
                }
                ++mCompleted;
            }
            mCv.notify_all();
        }
    }
    bool ShaderCacheWriter::Apply(const Pending &pending, std::string &error)
    {
        ShaderBlobStore store(mLocal, mLocal);
        if (!store.Store(pending.key, pending.result)) {
            error = store.LastError();
            return false;
        }
        const auto directory = std::string(ShaderTargetDirName(pending.key.target));
        if (!UpsertLocalArtifact(*mLocal, directory, pending.relativePath, pending.schema, pending.schemaFp, pending.key, pending.result,
                                 pending.deps, &error)) {
            return false;
        }
        ShaderCacheIndex index;
        if (!LoadIndex(*mLocal, directory, index)) {
            error = "published shader index cannot be read";
            return false;
        }
        Publish(pending.key.target, std::move(index));
        return true;
    }
    void ShaderCacheWriter::Publish(uint32_t target, ShaderCacheIndex index)
    {
        const auto                  snapshot = std::make_shared<const ShaderCacheIndex>(std::move(index));
        std::lock_guard<std::mutex> lock(mMutex);
        mSnapshot[target] = snapshot;
    }
    bool ShaderCacheWriter::LookupArtifact(const ShaderCacheKey &key, ShaderCacheArtifact &out) const
    {
        if (key.target >= kTargetCount) {
            return false;
        }
        std::shared_ptr<const ShaderCacheIndex> snapshot;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            snapshot = mSnapshot[key.target];
        }
        const auto *entry  = snapshot != nullptr ? FindPathEntry(*snapshot, key.sourcePath) : nullptr;
        const auto *record = entry != nullptr ? FindArtifact(*entry, key) : nullptr;
        if (record == nullptr) {
            return false;
        }
        out = *record;
        return true;
    }
    bool ShaderCacheWriter::LookupSchema(uint32_t target, const std::string &path, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp) const
    {
        if (target >= kTargetCount) {
            return false;
        }
        std::shared_ptr<const ShaderCacheIndex> snapshot;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            snapshot = mSnapshot[target];
        }
        const auto *entry = snapshot != nullptr ? FindPathEntry(*snapshot, path) : nullptr;
        if (entry == nullptr) {
            return false;
        }
        outSchema   = entry->schema;
        outSchemaFp = entry->schemaFp;
        return true;
    }
} // namespace sky::aurora
