#pragma once

#include <aurora/shader/ShaderCompile.h>
#include <aurora/shader/ShaderHash.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {
    struct ShaderPublishedRequest {
        std::string                     sourceEntry;
        ShaderStageFlagBit              stage       = ShaderStageFlagBit::VS;
        uint64_t                        variantHash = 0;
        uint64_t                        layoutFp    = 0;
        uint64_t                        schemaFp    = 0;
        std::vector<ShaderVariantValue> variants;
        ShaderTargetEnvironment         environment;
        bool                            operator==(const ShaderPublishedRequest &) const = default;
    };

    struct ShaderCacheArtifact {
        uint64_t                 compileHash = 0;
        uint64_t                 artifactId  = 0;
        ShaderCacheKey           key;
        ShaderStageIdentity      identity;
        std::vector<std::string> sourceDeps;
        std::vector<std::vector<ShaderVariantValue>> selections;
    };
    struct ShaderCachePathEntry {
        uint64_t                                              schemaFp = 0;
        ShaderVariantSchema                                   schema;
        std::vector<std::pair<uint64_t, ShaderCacheArtifact>> artifacts;
    };
    struct ShaderCacheIndex {
        static constexpr uint32_t                                 kFormatVersion = 10;
        uint32_t                                                  formatVersion  = kFormatVersion;
        std::vector<std::pair<std::string, ShaderCachePathEntry>> paths;
    };
    using ShaderCacheBlob = ShaderStageArtifact;

    const char                 *ShaderTargetDirName(uint32_t target);
    uint64_t                    CompileHash(const ShaderCacheKey &key);
    ShaderPublishedRequest      PublishedRequest(const ShaderCacheKey &key);
    uint64_t                    ArtifactKey(const ShaderPublishedRequest &request);
    bool                        ValidateCacheRecord(const ShaderCacheArtifact &record, std::string &error);
    bool                        ArtifactMatchesKey(const ShaderStageArtifact &artifact, const ShaderCacheKey &key);
    ShaderCachePathEntry       *FindPathEntry(ShaderCacheIndex &index, const std::string &path);
    const ShaderCachePathEntry *FindPathEntry(const ShaderCacheIndex &index, const std::string &path);
    const ShaderCacheArtifact  *FindArtifact(const ShaderCachePathEntry &entry, const ShaderCacheKey &key);
    const ShaderCacheArtifact  *FindPublishedArtifact(const ShaderCachePathEntry &entry, const ShaderPublishedRequest &request);

    std::vector<uint8_t> EncodeIndex(const ShaderCacheIndex &index);
    bool                 DecodeIndex(const std::vector<uint8_t> &bytes, ShaderCacheIndex &out);
    std::vector<uint8_t> EncodeBlob(const ShaderCacheBlob &blob);
    bool                 DecodeBlob(const std::vector<uint8_t> &bytes, ShaderCacheBlob &out);

    // Rename is required: a failed publication leaves the previous file intact.
    bool WriteFileAtomic(sky::IFileSystem &root, const std::string &path, const std::vector<uint8_t> &bytes);
    bool UpsertArtifact(ShaderCacheIndex               &index,
                        const std::string              &path,
                        const ShaderVariantSchema      &schema,
                        uint64_t                        schemaFp,
                        const ShaderCacheKey           &key,
                        const ShaderCompileResult      &result,
                        const std::vector<std::string> &deps,
                        std::string                    *error = nullptr);
    bool UpsertLocalArtifact(sky::IFileSystem               &local,
                             const std::string              &subDir,
                             const std::string              &path,
                             const ShaderVariantSchema      &schema,
                             uint64_t                        schemaFp,
                             const ShaderCacheKey           &key,
                             const ShaderCompileResult      &result,
                             const std::vector<std::string> &deps,
                             std::string                    *error = nullptr);
    bool LoadIndex(sky::IFileSystem &root, const std::string &subDir, ShaderCacheIndex &out);
    bool SaveIndex(sky::IFileSystem &root, const std::string &subDir, const ShaderCacheIndex &index);

    class ShaderBlobStore : public ShaderCache {
    public:
        ShaderBlobStore(sky::IFileSystem *readRoot, sky::IFileSystem *writeRoot, uint64_t layoutFp = 0, uint64_t toolchainFp = 0);
        bool               Load(const ShaderCacheKey &key, ShaderCompileResult &out) override;
        bool               LoadArtifact(const ShaderCacheArtifact &record, ShaderCompileResult &out);
        bool               Store(const ShaderCacheKey &key, const ShaderCompileResult &result) override;
        const std::string &LastError() const
        {
            return mLastError;
        }

    private:
        std::string       BlobPath(uint64_t artifactId, uint32_t target) const;
        sky::IFileSystem *mReadRoot  = nullptr;
        sky::IFileSystem *mWriteRoot = nullptr;
        std::string       mLastError;
    };
} // namespace sky::aurora
