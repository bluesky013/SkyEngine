//
// ShaderCacheWriter implementation.
//

#include <aurora/shader/ShaderCacheWriter.h>

#include <core/file/FileSystem.h>

namespace sky::aurora {

    namespace {

        uint32_t TargetSlot(uint32_t target)
        {
            return target < ShaderCacheWriter::kTargetCount ? target : 0;
        }

    } // namespace

    ShaderCacheWriter::ShaderCacheWriter(sky::IFileSystem *localRoot) : mLocal(localRoot)
    {
    }

    ShaderCacheWriter::~ShaderCacheWriter()
    {
        Stop();
    }

    void ShaderCacheWriter::Start()
    {
        if (mLocal == nullptr) {
            return;
        }
        for (uint32_t t = 0; t < kTargetCount; ++t) {
            ShaderCacheIndex index;
            if (LoadIndex(*mLocal, ShaderTargetDirName(t), index)) {
                mLocalIndex[t] = std::move(index);
            }
            Publish(t);
        }
        mRunning = true;
        mThread  = std::thread([this] { ThreadMain(); });
    }

    void ShaderCacheWriter::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mRunning) {
                return;
            }
            mRunning = false;
        }
        mCv.notify_all();
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    void ShaderCacheWriter::Submit(Pending pending)
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mQueue.push_back(std::move(pending));
        }
        mCv.notify_one();
    }

    void ShaderCacheWriter::Flush()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (!mRunning) {
            return;
        }
        mFlushRequested = true;
        mCv.notify_all();
        mCv.wait(lock, [this] { return !mFlushRequested; });
    }

    void ShaderCacheWriter::ThreadMain()
    {
        for (;;) {
            std::vector<Pending> batch;
            bool                 doFlush = false;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCv.wait(lock, [this] { return !mRunning || !mQueue.empty() || mFlushRequested; });
                if (!mRunning && mQueue.empty()) {
                    break;
                }
                batch.assign(std::make_move_iterator(mQueue.begin()), std::make_move_iterator(mQueue.end()));
                mQueue.clear();
                doFlush = mFlushRequested;
            }

            ApplyBatch(batch);

            if (doFlush) {
                std::lock_guard<std::mutex> lock(mMutex);
                mFlushRequested = false;
                mCv.notify_all();
            }
        }
    }

    void ShaderCacheWriter::ApplyBatch(const std::vector<Pending> &batch)
    {
        if (mLocal == nullptr || batch.empty()) {
            return;
        }
        ShaderBlobStore store(mLocal, mLocal, 0, 0);

        bool dirty[kTargetCount] = {};
        for (const auto &p : batch) {
            store.Store(p.key, p.result);
            const uint32_t slot = TargetSlot(p.key.target);
            UpsertArtifact(mLocalIndex[slot], p.relativePath, p.schema, p.schemaFp, p.key, p.sourceHash, p.deps);
            dirty[slot] = true;
        }
        for (uint32_t t = 0; t < kTargetCount; ++t) {
            if (dirty[t]) {
                SaveIndex(*mLocal, ShaderTargetDirName(t), mLocalIndex[t]);
                Publish(t);
            }
        }
    }

    void ShaderCacheWriter::Publish(uint32_t target)
    {
        const uint32_t              slot = TargetSlot(target);
        auto                        snap = std::make_shared<const ShaderCacheIndex>(mLocalIndex[slot]);
        std::lock_guard<std::mutex> lock(mMutex);
        mSnapshot[slot] = std::move(snap);
    }

    bool ShaderCacheWriter::LookupArtifact(uint32_t             target,
                                           const std::string   &path,
                                           uint64_t             schemaFp,
                                           uint64_t             variantHash,
                                           uint32_t             stage,
                                           uint64_t             entryHash,
                                           ShaderCacheArtifact &out) const
    {
        std::shared_ptr<const ShaderCacheIndex> snap;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            snap = mSnapshot[TargetSlot(target)];
        }
        if (snap == nullptr) {
            return false;
        }
        const ShaderCachePathEntry *pe = FindPathEntry(*snap, path);
        if (pe == nullptr || pe->schemaFp != schemaFp) {
            return false;
        }
        const ShaderCacheArtifact *art = FindArtifact(*pe, variantHash, target, stage, entryHash);
        if (art == nullptr) {
            return false;
        }
        out = *art;
        return true;
    }

    bool ShaderCacheWriter::LookupSchema(uint32_t target, const std::string &path, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp) const
    {
        std::shared_ptr<const ShaderCacheIndex> snap;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            snap = mSnapshot[TargetSlot(target)];
        }
        if (snap == nullptr) {
            return false;
        }
        const ShaderCachePathEntry *pe = FindPathEntry(*snap, path);
        if (pe == nullptr || pe->schemaFp == 0) {
            return false;
        }
        outSchema   = pe->schema;
        outSchemaFp = pe->schemaFp;
        return true;
    }

} // namespace sky::aurora
