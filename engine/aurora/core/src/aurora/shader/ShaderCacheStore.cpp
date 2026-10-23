#include <aurora/shader/ShaderCacheStore.h>

#include <core/archive/StreamArchive.h>
#include <core/file/FileSystem.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>

namespace sky::aurora {
    namespace {
        std::string Hex64(uint64_t value)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string    result(16, '0');
            for (int i = 15; i >= 0; --i) {
                result[i] = digits[value & 15];
                value >>= 4;
            }
            return result;
        }

        class ScopedDirLock {
        public:
            explicit ScopedDirLock(sky::IFileSystem &fs)
            {
                const auto root = fs.GetPath().GetStr();
                if (root.empty()) {
                    return;
                }
                std::error_code ec;
                std::filesystem::create_directories(root, ec);
                if (ec) {
                    return;
                }
                lockPath = std::filesystem::path(root) / ".shadercache.lock";
                for (uint32_t i = 0; i < 200; ++i) {
                    ec.clear();
                    if (std::filesystem::create_directory(lockPath, ec)) {
                        held = true;
                        return;
                    }
                    if (ec && ec != std::errc::file_exists) {
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            ~ScopedDirLock()
            {
                if (held) {
                    std::error_code ec;
                    std::filesystem::remove(lockPath, ec);
                }
            }
            bool Held() const
            {
                return held;
            }

        private:
            std::filesystem::path lockPath;
            bool                  held = false;
        };

        uint64_t MixString(uint64_t hash, const std::string &value)
        {
            return HashMixBytes(HashMixU64(hash, value.size()), value);
        }
        uint64_t MixVariants(uint64_t hash, const std::vector<ShaderVariantValue> &variants)
        {
            hash = HashMixU64(hash, variants.size());
            for (const auto &variant : variants) {
                hash = MixString(hash, variant.name);
                hash = HashMixU64(hash, variant.value);
            }
            return hash;
        }
        bool Fail(std::string *error, const std::string &message)
        {
            if (error != nullptr) {
                *error = message;
            }
            return false;
        }
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
        auto hash = HashMixU64(kFnv1aBasis, ShaderCacheIndex::kFormatVersion);
        hash      = HashMixU64(hash, key.sourceHash);
        hash      = HashMixU64(hash, key.variantHash);
        hash      = HashMixU64(hash, key.layoutFp);
        hash      = HashMixU64(hash, key.schemaFp);
        hash      = HashMixU64(hash, key.toolchainFp);
        hash      = HashMixU64(hash, key.entryHash);
        hash      = HashMixU64(hash, key.target);
        hash      = HashMixU64(hash, key.stage);
        hash      = MixString(hash, key.sourcePath);
        hash      = MixString(hash, key.sourceEntry);
        hash      = MixVariants(hash, key.variants);
        hash      = HashMixU64(hash, ShaderEnvironmentFingerprint(key.environment));
        return HashMixU64(hash, ShaderProducerFingerprint(key.producer));
    }
    ShaderPublishedRequest PublishedRequest(const ShaderCacheKey &key)
    {
        return {key.sourceEntry, static_cast<ShaderStageFlagBit>(key.stage), key.variantHash, key.layoutFp, key.schemaFp, key.variants,
                key.environment};
    }
    uint64_t ArtifactKey(const ShaderPublishedRequest &request)
    {
        auto hash = MixString(kFnv1aBasis, request.sourceEntry);
        hash      = HashMixU64(hash, static_cast<uint32_t>(request.stage));
        hash      = HashMixU64(hash, request.variantHash);
        hash      = HashMixU64(hash, request.layoutFp);
        hash      = HashMixU64(hash, request.schemaFp);
        hash      = MixVariants(hash, request.variants);
        return HashMixU64(hash, ShaderEnvironmentFingerprint(request.environment));
    }
    bool ArtifactMatchesKey(const ShaderStageArtifact &artifact, const ShaderCacheKey &key)
    {
        const auto &id = artifact.identity;
        return id.sourcePath == key.sourcePath && id.sourceEntry == key.sourceEntry && id.stage == static_cast<ShaderStageFlagBit>(key.stage) &&
               id.sourceHash == key.sourceHash && id.variantHash == key.variantHash && id.layoutFp == key.layoutFp && id.schemaFp == key.schemaFp &&
               id.variants == key.variants && id.environment == key.environment && id.producer == key.producer &&
               key.target == static_cast<uint32_t>(id.environment.target) && key.entryHash == HashEntryName(id.sourceEntry) &&
               key.toolchainFp == ShaderProducerFingerprint(id.producer);
    }
    bool ValidateCacheRecord(const ShaderCacheArtifact &record, std::string &error)
    {
        ShaderStageArtifact artifact;
        artifact.identity = record.identity;
        auto environment  = record.key.environment;
        if (record.artifactId == 0 || record.compileHash != CompileHash(record.key) || !ArtifactMatchesKey(artifact, record.key) ||
            !NormalizeShaderEnvironment(environment, error) || environment != record.key.environment) {
            if (error.empty()) {
                error = "invalid shader cache record identity";
            }
            return false;
        }
        if (record.key.variantHash != HashShaderVariants(record.key.variants)) {
            error = "shader cache variant fingerprint mismatch";
            return false;
        }
        for (size_t i = 0; i < record.key.variants.size(); ++i) {
            if (record.key.variants[i].name.empty() || (i && record.key.variants[i - 1].name >= record.key.variants[i].name)) {
                error = "shader cache variants are not canonical";
                return false;
            }
        }
        return true;
    }
    ShaderCachePathEntry *FindPathEntry(ShaderCacheIndex &index, const std::string &path)
    {
        for (auto &entry : index.paths) {
            if (entry.first == path) {
                return &entry.second;
            }
        }
        return nullptr;
    }
    const ShaderCachePathEntry *FindPathEntry(const ShaderCacheIndex &index, const std::string &path)
    {
        for (const auto &entry : index.paths) {
            if (entry.first == path) {
                return &entry.second;
            }
        }
        return nullptr;
    }
    const ShaderCacheArtifact *FindArtifact(const ShaderCachePathEntry &entry, const ShaderCacheKey &key)
    {
        const auto hash = CompileHash(key);
        for (const auto &artifact : entry.artifacts) {
            if (artifact.second.compileHash == hash && artifact.second.key == key) {
                return &artifact.second;
            }
        }
        return nullptr;
    }
    const ShaderCacheArtifact *FindPublishedArtifact(const ShaderCachePathEntry &entry, const ShaderPublishedRequest &request)
    {
        const auto hash = ArtifactKey(request);
        for (const auto &artifact : entry.artifacts) {
            if (artifact.first == hash && PublishedRequest(artifact.second.key) == request) {
                return &artifact.second;
            }
        }
        return nullptr;
    }
    bool WriteFileAtomic(sky::IFileSystem &root, const std::string &path, const std::vector<uint8_t> &bytes)
    {
        const std::string temporary = path + ".tmp";
        {
            const auto file = root.CreateOrOpenFile(sky::FilePath(temporary));
            if (file == nullptr) {
                return false;
            }
            const auto archive = file->WriteAsArchive();
            if (archive == nullptr || !archive->SaveRaw(reinterpret_cast<const char *>(bytes.data()), bytes.size())) {
                return false;
            }
        }
        return root.Rename(sky::FilePath(temporary), sky::FilePath(path));
    }
    bool LoadIndex(sky::IFileSystem &root, const std::string &subDir, ShaderCacheIndex &out)
    {
        const auto           file = root.OpenFile(sky::FilePath(subDir.empty() ? "index.bin" : subDir + "/index.bin"));
        std::vector<uint8_t> bytes;
        return file != nullptr && file->ReadBin(bytes) && DecodeIndex(bytes, out);
    }
    bool SaveIndex(sky::IFileSystem &root, const std::string &subDir, const ShaderCacheIndex &index)
    {
        const auto       bytes = EncodeIndex(index);
        ShaderCacheIndex validated;
        if (!DecodeIndex(bytes, validated)) {
            return false;
        }
        const ScopedDirLock lock(root);
        if (!lock.Held()) {
            return false;
        }
        root.MakeDir(sky::FilePath(subDir));
        return WriteFileAtomic(root, subDir.empty() ? "index.bin" : subDir + "/index.bin", bytes);
    }
    bool UpsertArtifact(ShaderCacheIndex               &index,
                        const std::string              &path,
                        const ShaderVariantSchema      &schema,
                        uint64_t                        schemaFp,
                        const ShaderCacheKey           &key,
                        const ShaderCompileResult      &result,
                        const std::vector<std::string> &deps,
                        std::string                    *error)
    {
        std::string validation;
        if (!ValidateShaderArtifact(result, validation) || !ArtifactMatchesKey(result, key) || path != key.sourcePath || schemaFp != key.schemaFp ||
            schemaFp != HashSchema(schema) || !schema.Validate(&validation)) {
            return Fail(error, validation.empty() ? "artifact does not match cache key/schema" : validation);
        }
        auto *entry = FindPathEntry(index, path);
        if (entry == nullptr) {
            index.paths.emplace_back(path, ShaderCachePathEntry{});
            entry = &index.paths.back().second;
        }
        if (entry->schemaFp != schemaFp) {
            entry->artifacts.clear();
        }
        entry->schema   = schema;
        entry->schemaFp = schemaFp;
        ShaderCacheArtifact record{CompileHash(key), result.artifactId, key, result.identity, deps};
        const auto          request = PublishedRequest(key);
        for (auto &artifact : entry->artifacts) {
            if (PublishedRequest(artifact.second.key) == request) {
                artifact = {ArtifactKey(request), std::move(record)};
                return true;
            }
        }
        entry->artifacts.emplace_back(ArtifactKey(request), std::move(record));
        return true;
    }
    bool UpsertLocalArtifact(sky::IFileSystem               &local,
                             const std::string              &subDir,
                             const std::string              &path,
                             const ShaderVariantSchema      &schema,
                             uint64_t                        schemaFp,
                             const ShaderCacheKey           &key,
                             const ShaderCompileResult      &result,
                             const std::vector<std::string> &deps,
                             std::string                    *error)
    {
        const ScopedDirLock lock(local);
        if (!lock.Held()) {
            return Fail(error, "shader index lock unavailable");
        }
        ShaderCacheIndex index;
        if (local.FileExist(sky::FilePath(subDir + "/index.bin")) && !LoadIndex(local, subDir, index)) {
            return Fail(error, "shader index is corrupt or requires recook");
        }
        if (!UpsertArtifact(index, path, schema, schemaFp, key, result, deps, error)) {
            return false;
        }
        local.MakeDir(sky::FilePath(subDir));
        if (!WriteFileAtomic(local, subDir + "/index.bin", EncodeIndex(index))) {
            return Fail(error, "shader index publication failed");
        }
        return true;
    }
    ShaderBlobStore::ShaderBlobStore(sky::IFileSystem *readRoot, sky::IFileSystem *writeRoot, uint64_t, uint64_t)
        : mReadRoot(readRoot), mWriteRoot(writeRoot)
    {
    }
    std::string ShaderBlobStore::BlobPath(uint64_t artifactId, uint32_t target) const
    {
        return std::string(ShaderTargetDirName(target)) + "/blobs/" + Hex64(artifactId) + ".bin";
    }
    bool ShaderBlobStore::Load(const ShaderCacheKey &key, ShaderCompileResult &out)
    {
        ShaderCacheIndex index;
        if (mReadRoot == nullptr || !LoadIndex(*mReadRoot, ShaderTargetDirName(key.target), index)) {
            mLastError = "shader index absent, corrupt or requires recook";
            return false;
        }
        const auto *entry  = FindPathEntry(index, key.sourcePath);
        const auto *record = entry != nullptr ? FindArtifact(*entry, key) : nullptr;
        if (record == nullptr) {
            mLastError = "compile key not found";
            return false;
        }
        return LoadArtifact(*record, out);
    }
    bool ShaderBlobStore::LoadArtifact(const ShaderCacheArtifact &record, ShaderCompileResult &out)
    {
        mLastError.clear();
        if (mReadRoot == nullptr || !ValidateCacheRecord(record, mLastError)) {
            return false;
        }
        const auto           file = mReadRoot->OpenFile(sky::FilePath(BlobPath(record.artifactId, record.key.target)));
        std::vector<uint8_t> bytes;
        ShaderCacheBlob      blob;
        if (file == nullptr || !file->ReadBin(bytes) || !DecodeBlob(bytes, blob)) {
            mLastError = "shader artifact absent or corrupt";
            return false;
        }
        if (blob.artifactId != record.artifactId || blob.identity != record.identity || !ArtifactMatchesKey(blob, record.key)) {
            mLastError = "shader index/artifact identity mismatch";
            return false;
        }
        ShaderCompileResult result;
        static_cast<ShaderStageArtifact &>(result) = std::move(blob);
        out                                        = std::move(result);
        return true;
    }
    bool ShaderBlobStore::Store(const ShaderCacheKey &key, const ShaderCompileResult &result)
    {
        mLastError.clear();
        if (mWriteRoot == nullptr) {
            mLastError = "no writable shader cache";
            return false;
        }
        if (!ValidateShaderArtifact(result, mLastError) || !ArtifactMatchesKey(result, key)) {
            if (mLastError.empty()) {
                mLastError = "compiled artifact/key identity mismatch";
            }
            return false;
        }
        const ScopedDirLock lock(*mWriteRoot);
        if (!lock.Held()) {
            mLastError = "shader blob lock unavailable";
            return false;
        }
        const auto directory = std::string(ShaderTargetDirName(key.target));
        mWriteRoot->MakeDir(sky::FilePath(directory + "/blobs"));
        if (!WriteFileAtomic(*mWriteRoot, BlobPath(result.artifactId, key.target), EncodeBlob(result))) {
            mLastError = "shader blob publication failed";
            return false;
        }
        return true;
    }
} // namespace sky::aurora
