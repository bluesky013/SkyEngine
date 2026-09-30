//
// ShaderCacheStore: content-addressed shader cache (blobs) + path index.
//
//   <root>/<target>/index.bin           path -> { schema, schemaFp, artifacts }
//   <root>/<target>/blobs/<compileHash> self-describing module blob (code + reflection)
//
// `ShaderBlobStore` implements the `ShaderCache` interface over a read root
// (offline, possibly a MultiFileSystem) and a writable local root.
//

#pragma once

#include <aurora/rhi/ShaderReflection.h>
#include <aurora/shader/ShaderCompile.h>
#include <aurora/shader/ShaderHash.h>
#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {

    // ---- index structures ----

    struct ShaderCacheArtifact {
        uint64_t                 compileHash = 0;
        uint64_t                 sourceHash  = 0;
        std::vector<std::string> sourceDeps;
    };

    struct ShaderCachePathEntry {
        uint64_t                                              schemaFp = 0;
        ShaderVariantSchema                                   schema;
        std::vector<std::pair<uint64_t, ShaderCacheArtifact>> artifacts; // key = ArtifactKey(...)
    };

    struct ShaderCacheIndex {
        static constexpr uint32_t kFormatVersion = 1;

        uint32_t                                                  formatVersion = kFormatVersion;
        uint64_t                                                  toolchainFp   = 0;
        uint64_t                                                  layoutFp      = 0;
        std::vector<std::pair<std::string, ShaderCachePathEntry>> paths; // normalizedPath
    };

    // ---- blob ----

    struct ShaderCacheBlob {
        uint32_t              target     = 0;
        uint64_t              sourceHash = 0;
        uint64_t              layoutFp   = 0;
        uint64_t              schemaFp   = 0;
        std::vector<uint32_t> data; // module code (SPIRV words / MSL text)
        ShaderReflection      reflection;
    };

    // ---- helpers ----

    // Per-backend cache subdirectory name (from ShaderTarget): vulkan / metal / d3d12.
    const char *ShaderTargetDirName(uint32_t target);

    uint64_t CompileHash(const ShaderCacheKey &key);
    // entry-level artifact identity: variant + target + stage + entry name
    uint64_t ArtifactKey(uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash);

    ShaderCachePathEntry       *FindPathEntry(ShaderCacheIndex &index, const std::string &path);
    const ShaderCachePathEntry *FindPathEntry(const ShaderCacheIndex &index, const std::string &path);
    ShaderCacheArtifact        *FindArtifact(ShaderCachePathEntry &entry, uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash);
    const ShaderCacheArtifact *
    FindArtifact(const ShaderCachePathEntry &entry, uint64_t variantHash, uint32_t target, uint32_t stage, uint64_t entryHash);

    // ---- codec (see ShaderCacheCodec.cpp) ----

    std::vector<uint8_t> EncodeIndex(const ShaderCacheIndex &index);
    bool                 DecodeIndex(const std::vector<uint8_t> &bytes, ShaderCacheIndex &out);

    std::vector<uint8_t> EncodeBlob(const ShaderCacheBlob &blob);
    bool                 DecodeBlob(const std::vector<uint8_t> &bytes, ShaderCacheBlob &out);

    // ---- filesystem-backed index ----

    // Write `path` via a temp file + rename when the filesystem supports it
    // (readers never observe a partial file); falls back to a direct write.
    bool WriteFileAtomic(sky::IFileSystem &root, const std::string &path, const std::vector<uint8_t> &bytes);

    // Insert/update an artifact entry in an in-memory index (entry-level keying).
    void UpsertArtifact(ShaderCacheIndex               &index,
                        const std::string              &path,
                        const ShaderVariantSchema      &schema,
                        uint64_t                        schemaFp,
                        const ShaderCacheKey           &key,
                        uint64_t                        sourceHash,
                        const std::vector<std::string> &deps);

    // Race-free local index read-modify-write (process mutex + dir lock).
    void UpsertLocalArtifact(sky::IFileSystem               &local,
                             const std::string              &subDir,
                             const std::string              &path,
                             const ShaderVariantSchema      &schema,
                             uint64_t                        schemaFp,
                             const ShaderCacheKey           &key,
                             uint64_t                        sourceHash,
                             const std::vector<std::string> &deps);

    // Read `<root>/<subDir>/index.bin`; returns false (and leaves out untouched) when absent.
    bool LoadIndex(sky::IFileSystem &root, const std::string &subDir, ShaderCacheIndex &out);
    // Write `<root>/<subDir>/index.bin` (local root).
    bool SaveIndex(sky::IFileSystem &root, const std::string &subDir, const ShaderCacheIndex &index);

    // ---- ShaderCache backend ----

    // Content-addressed blob store. Reads resolve against `readRoot` (offline
    // first; may be a MultiFileSystem), writes go to `writeRoot` (local).
    class ShaderBlobStore : public ShaderCache {
    public:
        ShaderBlobStore(sky::IFileSystem *readRoot, sky::IFileSystem *writeRoot, uint64_t layoutFp = 0, uint64_t toolchainFp = 0);

        bool Load(const ShaderCacheKey &key, ShaderCompileResult &out) override;
        void Store(const ShaderCacheKey &key, const ShaderCompileResult &result) override;

        const std::string &LastError() const
        {
            return mLastError;
        }

    private:
        std::string BlobPath(uint64_t compileHash, uint32_t target) const;

        sky::IFileSystem *mReadRoot    = nullptr;
        sky::IFileSystem *mWriteRoot   = nullptr;
        uint64_t          mLayoutFp    = 0;
        uint64_t          mToolchainFp = 0;
        std::string       mLastError;
    };

} // namespace sky::aurora
