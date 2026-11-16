#pragma once

#include <algorithm>
#include <chrono>
#include <framework/asset/AssetIndexFile.h>
#include <functional>
#include <thread>

namespace sky {
    inline FileOperationStatus
    AcquireManifestLock(const FileSystemPtr &filesystem, const FilePath &directory, FileLockPtr &out, const std::function<bool()> &cancel = {})
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        auto       delay    = std::chrono::milliseconds(2);
        for (;;) {
            if (cancel && cancel())
                return {FileOperationError::Busy, "Source operation cancelled"};
            auto status = filesystem->AcquireFileLock(directory / FilePath("assets.jsonl.lock"), out);
            if (status || status.error != FileOperationError::Busy)
                return status;
            if (std::chrono::steady_clock::now() >= deadline)
                return {FileOperationError::Busy, "Source manifest remained busy until the retry deadline"};
            std::this_thread::sleep_for(delay);
            delay = std::min(delay * 2, std::chrono::milliseconds(32));
        }
    }

    // Callers hold directory locks in lexical order; candidates stay private until atomic persistence succeeds.
    class SourceManifestTransaction {
    public:
        SourceManifestTransaction(AssetIndexFileCache &cache, FileSystemPtr filesystem, FilePath directory, std::function<bool()> cancel = {})
            : cache(cache), filesystem(std::move(filesystem)), directory(std::move(directory)), cancel(std::move(cancel))
        {
        }
        FileOperationStatus Open(bool writable = true)
        {
            if (writable) {
                if (const auto status = AcquireManifestLock(filesystem, directory, lock, cancel); !status)
                    return status;
            }
            return cache.Reload(filesystem, directory, manifest);
        }
        AssetFileWriteResult Commit(const AssetIndexFile &candidate)
        {
            if (cancel && cancel())
                return {{FileOperationError::Busy, "Source operation cancelled before commit"}, {}};
            if (!lock)
                return {{FileOperationError::Unsupported, "Manifest commit requires its write lock"}, {}};
            return cache.Save(filesystem, directory, candidate);
        }
        void Unlock()
        {
            lock.reset();
        }
        AssetIndexFile manifest{"file", "cook"};

    private:
        AssetIndexFileCache  &cache;
        FileSystemPtr         filesystem;
        FilePath              directory;
        FileLockPtr           lock;
        std::function<bool()> cancel;
    };
} // namespace sky
