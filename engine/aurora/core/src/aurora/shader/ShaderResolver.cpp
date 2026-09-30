//
// ShaderResolver implementation.
//

#include <aurora/shader/ShaderResolver.h>

#include <aurora/shader/ShaderCacheWriter.h>
#include <aurora/shader/ShaderFileSystem.h>

#include <core/file/FileSystem.h>

#include <unordered_set>

namespace sky::aurora {

    namespace {

        std::string DirOf(const std::string &path)
        {
            const auto pos = path.find_last_of('/');
            return pos == std::string::npos ? std::string() : path.substr(0, pos + 1);
        }

        bool ParseInclude(const std::string &line, std::string &out)
        {
            const auto pos = line.find("#include");
            if (pos == std::string::npos) {
                return false;
            }
            const auto q1 = line.find('"', pos);
            if (q1 == std::string::npos) {
                return false;
            }
            const auto q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) {
                return false;
            }
            out = line.substr(q1 + 1, q2 - q1 - 1);
            return true;
        }

        bool
        HashRec(ShaderFileSystem &fs, const std::string &path, std::unordered_set<std::string> &visited, uint64_t &hash, std::string *rootContent)
        {
            if (visited.count(path) != 0) {
                return true;
            }
            visited.insert(path);

            std::string content;
            if (!fs.ReadFile(path, content)) {
                return false;
            }
            if (rootContent != nullptr) {
                *rootContent = content;
            }
            hash = HashMixBytes(hash, path);
            hash = HashMixBytes(hash, content);

            size_t start = 0;
            while (start < content.size()) {
                const auto        end  = content.find('\n', start);
                const std::string line = content.substr(start, end == std::string::npos ? std::string::npos : end - start);
                std::string       inc;
                if (ParseInclude(line, inc)) {
                    std::string dummy;
                    HashRec(fs, DirOf(path) + inc, visited, hash, nullptr);
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

    } // namespace

    ShaderResolver::ShaderResolver(
        sky::IFileSystem *offlineRoot, sky::IFileSystem *localRoot, ShaderFileSystem &sourceFs, uint64_t layoutFp, uint64_t toolchainFp)
        : mOffline(offlineRoot), mLocal(localRoot), mSourceFs(sourceFs), mLayoutFp(layoutFp), mToolchainFp(toolchainFp)
    {
    }

    bool ShaderResolver::HashSourceClosure(
        ShaderFileSystem &fs, const std::string &relativePath, const std::vector<std::string> &extraDeps, uint64_t &outHash, std::string &outSource)
    {
        std::unordered_set<std::string> visited;
        std::string                     root;
        uint64_t                        hash = kFnv1aBasis;
        if (!HashRec(fs, relativePath, visited, hash, &root)) {
            return false;
        }
        outSource = std::move(root);
        // companion schema participates in the source hash
        std::string companion;
        if (fs.ReadFile(relativePath + ".json", companion)) {
            hash = HashMixBytes(hash, companion);
        }
        for (const auto &dep : extraDeps) {
            std::string content;
            if (fs.ReadFile(dep, content)) {
                hash = HashMixBytes(hash, dep);
                hash = HashMixBytes(hash, content);
            }
        }
        outHash = hash;
        return true;
    }
    bool ShaderResolver::ResolveSchema(
        const std::string &relativePath, uint32_t target, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp, std::string *error)
    {
        ShaderCompanion companion;
        if (LoadShaderCompanion(mSourceFs, relativePath, companion, nullptr)) {
            outSchema   = companion.schema;
            outSchemaFp = companion.fingerprint;
            return true;
        }

        const std::string subDir = ShaderTargetDirName(target);
        for (sky::IFileSystem *root : {mOffline, mLocal}) {
            if (root == nullptr) {
                continue;
            }
            if (root == mLocal && mWriter != nullptr) {
                if (mWriter->LookupSchema(target, relativePath, outSchema, outSchemaFp)) {
                    return true;
                }
                continue;
            }
            ShaderCacheIndex index;
            if (!LoadIndex(*root, subDir, index)) {
                continue;
            }
            if (index.formatVersion != ShaderCacheIndex::kFormatVersion || index.toolchainFp != mToolchainFp || index.layoutFp != mLayoutFp) {
                continue;
            }
            const ShaderCachePathEntry *pe = FindPathEntry(index, relativePath);
            if (pe != nullptr && pe->schemaFp != 0) {
                outSchema   = pe->schema;
                outSchemaFp = pe->schemaFp;
                return true;
            }
        }

        if (error != nullptr) {
            *error = "schema not found: " + relativePath;
        }
        return false;
    }
    bool ShaderResolver::CacheUsable(
        sky::IFileSystem &root, const Request &req, uint64_t schemaFp, bool haveSource, uint64_t sourceHash, uint64_t &outSourceHash)
    {
        ShaderCacheIndex index;
        if (!LoadIndex(root, ShaderTargetDirName(req.target), index)) {
            return false;
        }
        if (index.formatVersion != ShaderCacheIndex::kFormatVersion || index.toolchainFp != mToolchainFp || index.layoutFp != mLayoutFp) {
            return false;
        }
        const ShaderCachePathEntry *pe = FindPathEntry(index, req.relativePath);
        if (pe == nullptr || pe->schemaFp != schemaFp) {
            return false;
        }
        const ShaderCacheArtifact *art = FindArtifact(*pe, req.variantHash, req.target, static_cast<uint32_t>(req.stage), HashEntryName(req.entry));
        if (art == nullptr) {
            return false;
        }
        if (haveSource && art->sourceHash != sourceHash) {
            return false;
        }
        outSourceHash = art->sourceHash;
        return true;
    }
    ShaderResolveStatus
    ShaderResolver::PrepareShader(const Request &req, ShaderCompileResult &outResult, ShaderCompileTask &outTask, std::string *error)
    {
        ShaderVariantSchema        localSchema;
        uint64_t                   schemaFp = req.schemaFp;
        const ShaderVariantSchema *schema   = req.schema;
        if (schema == nullptr) {
            if (!ResolveSchema(req.relativePath, req.target, localSchema, schemaFp, error)) {
                return ShaderResolveStatus::Failed;
            }
            schema = &localSchema;
        }

        uint64_t    sourceHash = 0;
        std::string source;
        const bool  haveSource = HashSourceClosure(mSourceFs, req.relativePath, req.depends, sourceHash, source);

        auto makeKey = [&](uint64_t effectiveSourceHash) {
            ShaderCacheKey k{};
            k.sourceHash  = effectiveSourceHash;
            k.variantHash = req.variantHash;
            k.layoutFp    = mLayoutFp;
            k.schemaFp    = schemaFp;
            k.toolchainFp = mToolchainFp;
            k.target      = req.target;
            k.entryHash   = HashEntryName(req.entry);
            k.stage       = static_cast<uint32_t>(req.stage);
            return k;
        };

        // 1) offline cache
        if (mOffline != nullptr) {
            uint64_t effectiveSourceHash = sourceHash;
            if (CacheUsable(*mOffline, req, schemaFp, haveSource, sourceHash, effectiveSourceHash)) {
                ShaderBlobStore store(mOffline, mLocal, mLayoutFp, mToolchainFp);
                if (store.Load(makeKey(effectiveSourceHash), outResult)) {
                    return ShaderResolveStatus::Ready;
                }
            }
        }

        // 2) source compile -> hand the caller a self-contained task (no threading
        //    decision here: inline render-parallel or async worker is up to caller)
        IShaderCompiler *compiler = ShaderCompilerFactory::Get().GetCompiler();
        if (haveSource && compiler != nullptr) {
            outTask              = ShaderCompileTask{};
            outTask.key          = makeKey(sourceHash);
            outTask.source       = std::move(source);
            outTask.entry        = req.entry;
            outTask.stage        = req.stage;
            outTask.target       = static_cast<ShaderTarget>(req.target);
            outTask.variant      = req.variant;
            outTask.schema       = *schema;
            outTask.fileSystem   = &mSourceFs;
            outTask.relativePath = req.relativePath;
            outTask.deps         = req.depends;
            outTask.schemaFp     = schemaFp;
            outTask.sourceHash   = sourceHash;
            return ShaderResolveStatus::Compile;
        }

        // 3) local cache
        if (mLocal != nullptr) {
            uint64_t effectiveSourceHash = sourceHash;
            bool     usable              = false;
            if (mWriter != nullptr) {
                ShaderCacheArtifact art;
                if (mWriter->LookupArtifact(req.target, req.relativePath, schemaFp, req.variantHash, static_cast<uint32_t>(req.stage),
                                            HashEntryName(req.entry), art) &&
                    (!haveSource || art.sourceHash == sourceHash)) {
                    effectiveSourceHash = art.sourceHash;
                    usable              = true;
                }
            } else {
                usable = CacheUsable(*mLocal, req, schemaFp, haveSource, sourceHash, effectiveSourceHash);
            }
            if (usable) {
                ShaderBlobStore store(mLocal, mLocal, mLayoutFp, mToolchainFp);
                if (store.Load(makeKey(effectiveSourceHash), outResult)) {
                    return ShaderResolveStatus::Ready;
                }
            }
        }

        if (error != nullptr && error->empty()) {
            *error = (haveSource && compiler == nullptr) ? "no compiler registered (cache miss)" : "shader missing";
        }
        return ShaderResolveStatus::Failed;
    }

    void ShaderResolver::CommitCompile(const ShaderCompileTask &task, const ShaderCompileResult &result)
    {
        if (mWriter != nullptr) {
            ShaderCacheWriter::Pending pending;
            pending.key          = task.key;
            pending.result       = result;
            pending.schema       = task.schema;
            pending.schemaFp     = task.schemaFp;
            pending.sourceHash   = task.sourceHash;
            pending.relativePath = task.relativePath;
            pending.deps         = task.deps;
            mWriter->Submit(std::move(pending));
        } else if (mLocal != nullptr) {
            ShaderBlobStore store(mLocal, mLocal, mLayoutFp, mToolchainFp);
            store.Store(task.key, result);
            UpsertLocalArtifact(*mLocal, ShaderTargetDirName(task.key.target), task.relativePath, task.schema, task.schemaFp, task.key,
                                task.sourceHash, task.deps);
        }
    }

    bool ShaderResolver::ResolveShader(const Request &req, ShaderCompileResult &out, std::string *error)
    {
        ShaderCompileTask         task;
        const ShaderResolveStatus status = PrepareShader(req, out, task, error);
        if (status == ShaderResolveStatus::Ready) {
            return true;
        }
        if (status == ShaderResolveStatus::Failed) {
            return false;
        }

        IShaderCompiler    *compiler = ShaderCompilerFactory::Get().GetCompiler();
        ShaderCompileResult result;
        if (!task.Run(*compiler, result, error)) {
            return false;
        }
        CommitCompile(task, result);
        out = std::move(result);
        return true;
    }

    ShaderResolveStatus
    ShaderResolver::ResolveShaderAsync(const Request &req, ShaderCompileResult &outResult, ShaderCompileFuture &outFuture, std::string *error)
    {
        ShaderCompileTask         task;
        const ShaderResolveStatus status = PrepareShader(req, outResult, task, error);
        if (status != ShaderResolveStatus::Compile) {
            return status;
        }

        auto state       = std::make_shared<ShaderCompileFuture::State>();
        state->task      = std::move(task);
        state->compiler  = ShaderCompilerFactory::Get().GetCompiler();
        state->commit    = [this](const ShaderCompileTask &t, const ShaderCompileResult &r) { CommitCompile(t, r); };
        outFuture.mState = std::move(state);
        return ShaderResolveStatus::Compile;
    }

} // namespace sky::aurora
