//
// ShaderCacheStore: blob store + index IO + atomic write helpers.
// Binary (de)serialization lives in ShaderCacheCodec.cpp.
//

#include <aurora/shader/ShaderCacheStore.h>

#include <core/archive/StreamArchive.h>
#include <core/file/FileSystem.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>

namespace sky::aurora {

    namespace {

        std::string Hex64(uint64_t v)
        {
            static const char *digits = "0123456789abcdef";
            std::string        s(16, '0');
            for (int i = 15; i >= 0; --i) {
                s[i] = digits[v & 0xFu];
                v >>= 4;
            }
            return s;
        }

        // Serializes writes within a process (recursive: UpsertLocalArtifact
        // holds it across LoadIndex + SaveIndex, which locks again).
        std::recursive_mutex &WriteMutex()
        {
            static std::recursive_mutex mutex;
            return mutex;
        }

        // Best-effort cross-process directory lock (atomic directory create).
        class ScopedDirLock {
        public:
            explicit ScopedDirLock(sky::IFileSystem &fs)
            {
                const std::string root = fs.GetPath().GetStr();
                if (root.empty()) {
                    return; // non-path filesystem: in-process mutex only
                }
                mLockPath = std::filesystem::path(root) / ".shadercache.lock";
                for (int i = 0; i < 200; ++i) {
                    std::error_code ec;
                    if (std::filesystem::create_directory(mLockPath, ec)) {
                        mHeld = true;
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                // timeout: proceed (last-writer-wins) rather than fail the build
            }

            ~ScopedDirLock()
            {
                if (mHeld) {
                    std::error_code ec;
                    std::filesystem::remove(mLockPath, ec);
                }
            }

            ScopedDirLock(const ScopedDirLock &)            = delete;
            ScopedDirLock &operator=(const ScopedDirLock &) = delete;

        private:
            std::filesystem::path mLockPath;
            bool                  mHeld = false;
        };

    } // namespace

    const char *ShaderTargetDirName(uint32_t target)
    {
        switch (static_cast<ShaderTarget>(target)) {
        case ShaderTarget::SPIRV: return "vulkan";
        case ShaderTarget::MSL: return "metal";
        case ShaderTarget::DXIL: return "d3d12";
        }
        return "unknown";
    }

    uint64_t CompileHash(const ShaderCacheKey &key)
    {
        uint64_t hash = kFnv1aBasis;
        hash          = HashMixU64(hash, key.sourceHash);
        hash          = HashMixU64(hash, key.variantHash);
        hash          = HashMixU64(hash, key.layoutFp);
        hash          = HashMixU64(hash, key.schemaFp);
        hash          = HashMixU64(hash, key.toolchainFp);
        hash          = HashMixU64(hash, key.entryHash);
        hash          = HashMixU64(hash, key.target);
        hash          = HashMixU64(hash, key.stage);
        return hash;
    }

    uint64_t ArtifactKey(uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash)
    {
        uint64_t hash = kFnv1aBasis;
        hash          = HashMixU64(hash, variantHash);
        hash          = HashMixU64(hash, target);
        hash          = HashMixU64(hash, stage);
        hash          = HashMixU64(hash, entryHash);
        return hash;
    }

    ShaderCachePathEntry *FindPathEntry(ShaderCacheIndex &index, const std::string &path)
    {
        for (auto &p : index.paths) {
            if (p.first == path) {
                return &p.second;
            }
        }
        return nullptr;
    }

    const ShaderCachePathEntry *FindPathEntry(const ShaderCacheIndex &index, const std::string &path)
    {
        for (const auto &p : index.paths) {
            if (p.first == path) {
                return &p.second;
            }
        }
        return nullptr;
    }

    ShaderCacheArtifact *FindArtifact(ShaderCachePathEntry &entry, uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash)
    {
        const uint64_t key = ArtifactKey(variantHash, target, stage, entryHash);
        for (auto &a : entry.artifacts) {
            if (a.first == key) {
                return &a.second;
            }
        }
        return nullptr;
    }

    const ShaderCacheArtifact *
    FindArtifact(const ShaderCachePathEntry &entry, uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash)
    {
        const uint64_t key = ArtifactKey(variantHash, target, stage, entryHash);
        for (const auto &a : entry.artifacts) {
            if (a.first == key) {
                return &a.second;
            }
        }
        return nullptr;
    }

    bool WriteFileAtomic(sky::IFileSystem &root, const std::string &path, const std::vector<uint8_t> &bytes)
    {
        auto writeDirect = [&](const std::string &p) {
            sky::FilePtr file = root.CreateOrOpenFile(sky::FilePath(p));
            if (file == nullptr) {
                return false;
            }
            const auto archive = file->WriteAsArchive();
            if (archive == nullptr) {
                return false;
            }
            return archive->SaveRaw(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        };

        const std::string tmp = path + ".tmp";
        if (writeDirect(tmp) && root.Rename(sky::FilePath(tmp), sky::FilePath(path))) {
            return true;
        }
        return writeDirect(path); // filesystem without rename: direct overwrite
    }

    bool LoadIndex(sky::IFileSystem &root, const std::string &subDir, ShaderCacheIndex &out)
    {
        const std::string path = subDir.empty() ? "index.bin" : subDir + "/index.bin";
        sky::FilePtr      file = root.OpenFile(sky::FilePath(path));
        if (file == nullptr) {
            return false;
        }
        std::vector<uint8_t> bytes;
        if (!file->ReadBin(bytes)) {
            return false;
        }
        return DecodeIndex(bytes, out);
    }

    bool SaveIndex(sky::IFileSystem &root, const std::string &subDir, const ShaderCacheIndex &index)
    {
        std::lock_guard<std::recursive_mutex> guard(WriteMutex());
        ScopedDirLock                         lock(root);

        const std::string path = subDir.empty() ? "index.bin" : subDir + "/index.bin";
        if (!subDir.empty()) {
            root.MakeDir(sky::FilePath(subDir));
        }
        return WriteFileAtomic(root, path, EncodeIndex(index));
    }

    void UpsertArtifact(ShaderCacheIndex               &index,
                        const std::string              &path,
                        const ShaderVariantSchema      &schema,
                        uint64_t                        schemaFp,
                        const ShaderCacheKey           &key,
                        uint64_t                        sourceHash,
                        const std::vector<std::string> &deps)
    {
        index.formatVersion = ShaderCacheIndex::kFormatVersion;
        index.layoutFp      = key.layoutFp;
        index.toolchainFp   = key.toolchainFp;

        ShaderCachePathEntry *pe = FindPathEntry(index, path);
        if (pe == nullptr) {
            index.paths.emplace_back(path, ShaderCachePathEntry{});
            pe = &index.paths.back().second;
        }
        pe->schemaFp = schemaFp;
        pe->schema   = schema;

        ShaderCacheArtifact *art = FindArtifact(*pe, key.variantHash, key.target, key.stage, key.entryHash);
        if (art == nullptr) {
            pe->artifacts.emplace_back(ArtifactKey(key.variantHash, key.target, key.stage, key.entryHash), ShaderCacheArtifact{});
            art = &pe->artifacts.back().second;
        }
        art->compileHash = CompileHash(key);
        art->sourceHash  = sourceHash;
        art->sourceDeps  = deps;
    }

    void UpsertLocalArtifact(sky::IFileSystem               &local,
                             const std::string              &subDir,
                             const std::string              &path,
                             const ShaderVariantSchema      &schema,
                             uint64_t                        schemaFp,
                             const ShaderCacheKey           &key,
                             uint64_t                        sourceHash,
                             const std::vector<std::string> &deps)
    {
        std::lock_guard<std::recursive_mutex> guard(WriteMutex());
        ShaderCacheIndex                      index;
        LoadIndex(local, subDir, index);
        UpsertArtifact(index, path, schema, schemaFp, key, sourceHash, deps);
        SaveIndex(local, subDir, index);
    }

    ShaderBlobStore::ShaderBlobStore(sky::IFileSystem *readRoot, sky::IFileSystem *writeRoot, uint64_t layoutFp, uint64_t toolchainFp)
        : mReadRoot(readRoot), mWriteRoot(writeRoot), mLayoutFp(layoutFp), mToolchainFp(toolchainFp)
    {
    }

    std::string ShaderBlobStore::BlobPath(uint64_t compileHash, uint32_t target) const
    {
        return std::string(ShaderTargetDirName(target)) + "/blobs/" + Hex64(compileHash) + ".bin";
    }

    bool ShaderBlobStore::Load(const ShaderCacheKey &key, ShaderCompileResult &out)
    {
        if (mReadRoot == nullptr) {
            mLastError = "no read root";
            return false;
        }
        const std::string path = BlobPath(CompileHash(key), key.target);
        sky::FilePtr      file = mReadRoot->OpenFile(sky::FilePath(path));
        if (file == nullptr) {
            mLastError = "blob not found";
            return false;
        }
        std::vector<uint8_t> bytes;
        if (!file->ReadBin(bytes)) {
            mLastError = "blob read failed";
            return false;
        }
        ShaderCacheBlob blob;
        if (!DecodeBlob(bytes, blob)) {
            mLastError = "blob decode failed";
            return false;
        }
        out.data       = std::move(blob.data);
        out.reflection = std::move(blob.reflection);
        out.errorInfo.clear();
        return true;
    }

    void ShaderBlobStore::Store(const ShaderCacheKey &key, const ShaderCompileResult &result)
    {
        if (mWriteRoot == nullptr) {
            mLastError = "no write root";
            return;
        }
        std::lock_guard<std::recursive_mutex> guard(WriteMutex());
        ScopedDirLock                         lock(*mWriteRoot);

        ShaderCacheBlob blob;
        blob.target     = key.target;
        blob.sourceHash = key.sourceHash;
        blob.layoutFp   = key.layoutFp;
        blob.schemaFp   = key.schemaFp;
        blob.data       = result.data;
        blob.reflection = result.reflection;

        const std::string targetDir = ShaderTargetDirName(key.target);
        const std::string path      = BlobPath(CompileHash(key), key.target);
        mWriteRoot->MakeDir(sky::FilePath(targetDir));
        mWriteRoot->MakeDir(sky::FilePath(targetDir + "/blobs"));
        WriteFileAtomic(*mWriteRoot, path, EncodeBlob(blob));
    }

} // namespace sky::aurora
