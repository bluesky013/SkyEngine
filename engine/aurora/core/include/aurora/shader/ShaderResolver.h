//
// ShaderResolver: turns a ShaderRef + variant into a compiled result, honoring
// the cache layering: companion > cache, and offline > source-compile > local.
//
// Compilation is exposed as a self-contained ShaderCompileTask so the caller can
// run it inline (render parallel, current frame) or async (worker, skippable).
//

#pragma once

#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderCacheStore.h>
#include <aurora/shader/ShaderCompanion.h>
#include <aurora/shader/ShaderCompileTask.h>
#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky {
    class IFileSystem;
}

namespace sky::aurora {

    class ShaderFileSystem;
    class ShaderCacheWriter;

    enum class ShaderResolveStatus : uint8_t {
        Ready,   // cache hit: result filled
        Compile, // cache miss: task ready to run (inline or async)
        Failed,  // no cache and no way to compile
    };

    class ShaderResolver {
    public:
        ShaderResolver(
            sky::IFileSystem *offlineRoot, sky::IFileSystem *localRoot, ShaderFileSystem &sourceFs, uint64_t layoutFp, uint64_t toolchainFp);

        // When set, local index/cache writes go through the dedicated writer
        // thread (async, serialized) instead of inline synchronous writes.
        void SetCacheWriter(ShaderCacheWriter *writer)
        {
            mWriter = writer;
        }

        struct Request {
            std::string                relativePath;
            std::string                entry;
            ShaderStageFlagBit         stage       = ShaderStageFlagBit::FS;
            uint32_t                   target      = 0;
            uint64_t                   variantHash = 0;
            const ShaderVariant       *variant     = nullptr;
            const ShaderVariantSchema *schema      = nullptr; // nullptr -> resolve
            uint64_t                   schemaFp    = 0;
            std::vector<std::string>   depends; // extra companion deps
        };

        // schema priority: source companion > offline index > local index
        bool ResolveSchema(
            const std::string &relativePath, uint32_t target, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp, std::string *error = nullptr);

        // Sync convenience: resolve a cache hit or compile inline on this thread.
        bool ResolveShader(const Request &req, ShaderCompileResult &out, std::string *error = nullptr);

        // Resolve up to the compile decision. On Ready fills `outResult`; on
        // Compile fills `outTask` (caller schedules it); on Failed sets `error`.
        ShaderResolveStatus
        PrepareShader(const Request &req, ShaderCompileResult &outResult, ShaderCompileTask &outTask, std::string *error = nullptr);

        // Persist a finished compile via the writer queue, or inline otherwise.
        void CommitCompile(const ShaderCompileTask &task, const ShaderCompileResult &result);

        // Prepare + (on Compile) wrap the task in a future the caller runs via
        // RunInline() or RunAsync(pool). On Ready fills `outResult`.
        ShaderResolveStatus
        ResolveShaderAsync(const Request &req, ShaderCompileResult &outResult, ShaderCompileFuture &outFuture, std::string *error = nullptr);

        // FNV-1a hash of the root source + included / declared deps.
        static bool HashSourceClosure(ShaderFileSystem               &fs,
                                      const std::string              &relativePath,
                                      const std::vector<std::string> &extraDeps,
                                      uint64_t                       &outHash,
                                      std::string                    &outSource);

    private:
        // Returns the artifact's stored sourceHash via `outSourceHash` (used to
        // reconstruct the compile key when the source is unavailable).
        bool
        CacheUsable(sky::IFileSystem &root, const Request &req, uint64_t schemaFp, bool haveSource, uint64_t sourceHash, uint64_t &outSourceHash);

        sky::IFileSystem  *mOffline;
        sky::IFileSystem  *mLocal;
        ShaderFileSystem  &mSourceFs;
        ShaderCacheWriter *mWriter = nullptr;
        uint64_t           mLayoutFp;
        uint64_t           mToolchainFp;
    };

} // namespace sky::aurora
