#pragma once

#include "SourceManifestTransaction.h"
#include <atomic>
#include <core/archive/MemoryStreamArchive.h>
#include <cstring>
#include <framework/asset/AssetProduct.h>
#include <limits>

namespace sky {
    inline uint64_t BuildInputHash(std::span<const uint8_t> bytes)
    {
        uint64_t hash = 14695981039346656037ULL;
        for (const auto value : bytes)
            hash = (hash ^ value) * 1099511628211ULL;
        return hash;
    }

    inline void AppendBuildKey(std::string &key, std::string_view value)
    {
        key += std::to_string(value.size()) + ":";
        key.append(value);
    }

    inline ProductStatus
    CaptureCookConfiguration(const std::vector<NativeFileSystemPtr> &roots, const std::vector<FilePath> &inputs, std::string &out)
    {
        std::string fingerprint;
        for (size_t mount = 0; mount < roots.size(); ++mount) {
            const auto &root = roots[mount];
            if (!root || !root->FileExist("configs"))
                continue;
            for (const auto &path : inputs) {
                if (!root->FileExist(FilePath("configs") / path))
                    continue;
                const auto           file = root->OpenFile(FilePath("configs") / path);
                std::vector<uint8_t> bytes;
                if (!file || !file->ReadBin(bytes))
                    return {ProductError::IOError, "Cook configuration file could not be read"};
                AppendBuildKey(fingerprint, std::to_string(mount));
                AppendBuildKey(fingerprint, path.GetStr());
                AppendBuildKey(fingerprint, std::to_string(BuildInputHash(bytes)));
            }
        }
        out = std::move(fingerprint);
        return {};
    }

    class BuildInputFile final : public IFile {
    public:
        BuildInputFile(std::string path, BinaryDataPtr bytes) : path(std::move(path)), bytes(std::move(bytes))
        {
        }
        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override
        {
            SKY_ASSERT(offset <= bytes->Size() && size <= bytes->Size() - offset);
            if (size)
                std::memcpy(out, bytes->Data() + offset, size);
        }
        bool ReadBin(std::vector<uint8_t> &out) override
        {
            out.clear();
            if (bytes->Size())
                out.assign(bytes->Data(), bytes->Data() + bytes->Size());
            return true;
        }
        BinaryDataPtr ReadBin() override
        {
            BinaryDataPtr copy = new BinaryData(static_cast<uint32_t>(bytes->Size()));
            if (bytes->Size())
                std::memcpy(copy->Data(), bytes->Data(), bytes->Size());
            return copy;
        }
        bool ReadString(std::string &out) override
        {
            out.clear();
            if (bytes->Size())
                out.assign(reinterpret_cast<const char *>(bytes->Data()), bytes->Size());
            return true;
        }
        IStreamArchivePtr ReadAsArchive() override
        {
            return new IMemoryArchive(bytes);
        }
        OStreamArchivePtr WriteAsArchive() override
        {
            return {};
        }
        uint64_t AppendData(const char *, uint64_t) override
        {
            return 0;
        }
        std::string GetPath() const override
        {
            return path;
        }

    private:
        std::string   path;
        BinaryDataPtr bytes;
    };

    class SourcePublicationGuard final : public AssetPublicationGuard {
    public:
        std::shared_ptr<std::recursive_mutex>  serialization;
        FileSystemPtr                          filesystem;
        FilePath                               path;
        Uuid                                   uuid;
        std::string                            cook;
        uint64_t                               hash = 0;
        std::shared_ptr<std::atomic<uint64_t>> revision;
        uint64_t                               expectedRevision    = 0;
        bool                                   writable            = false;
        bool                                   hadManifestIdentity = false;

        ProductStatus Acquire(std::unique_ptr<AssetPublicationScope> &out) const override
        {
            struct Scope final : AssetPublicationScope {
                std::unique_lock<std::recursive_mutex> serialization;
                FileLockPtr                            lock;
                explicit Scope(std::recursive_mutex &mutex) : serialization(mutex)
                {
                }
            };
            auto       scope     = std::make_unique<Scope>(*serialization);
            const auto cancelled = [this] { return revision->load(std::memory_order_acquire) != expectedRevision; };
            if (cancelled())
                return {ProductError::Stale, "Cook source incarnation or settings changed"};
            if (writable) {
                if (const auto status = AcquireManifestLock(filesystem, path.Parent(), scope->lock, cancelled); !status)
                    return {status.error == FileOperationError::Busy ? ProductError::Busy : ProductError::IOError, status.message};
            }
            if (const auto status = CheckSource(uuid, filesystem, path); !status)
                return status;
            out = std::move(scope);
            return {};
        }
        ProductStatus CheckSource(const Uuid &owner, const FileSystemPtr &fs, const FilePath &sourcePath) const override
        {
            if (owner != uuid || fs.Get() != filesystem.Get() || sourcePath != path)
                return {ProductError::Stale, "Cook source transaction does not match its captured owner"};
            const auto     cancelled = [this] { return revision->load(std::memory_order_acquire) != expectedRevision; };
            AssetIndexFile manifest("file", "cook");
            if (!AssetIndexFile::LoadChecked(filesystem, path.Parent() / FilePath("assets.jsonl"), manifest))
                return {ProductError::Stale, "Cook source manifest is unavailable or damaged"};
            const auto *entry = manifest.Find(path.FileName());
            if ((hadManifestIdentity && !entry) || (entry && (entry->id != uuid || entry->extra != cook)))
                return {ProductError::Stale, "Cook source identity or settings no longer match the captured input"};
            const auto           file = filesystem->OpenFile(path);
            std::vector<uint8_t> bytes;
            if (!file || !file->ReadBin(bytes) || BuildInputHash(bytes) != hash || cancelled())
                return {ProductError::Stale, "Cook source bytes changed or the source was removed"};
            return {};
        }
    };
} // namespace sky
