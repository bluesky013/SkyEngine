//
// ShaderResolver implementation.
//

#include <aurora/shader/ShaderResolver.h>

#include <aurora/shader/ShaderCacheWriter.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderUsage.h>

#include <core/file/FileSystem.h>

#include <algorithm>
#include <unordered_set>

namespace sky::aurora {

    namespace {

        std::string DirOf(const std::string &path)
        {
            const auto pos = path.find_last_of('/');
            return pos == std::string::npos ? std::string() : path.substr(0, pos + 1);
        }

        bool ParseInclude(const std::string &line, std::string &out, bool &rootRelative)
        {
            const auto pos = line.find("#include");
            if (pos == std::string::npos) {
                return false;
            }
            const auto q1 = line.find_first_not_of(" \t", pos + 8);
            if (q1 == std::string::npos || (line[q1] != '"' && line[q1] != '<')) {
                return false;
            }
            rootRelative  = line[q1] == '<';
            const auto q2 = line.find(rootRelative ? '>' : '"', q1 + 1);
            if (q2 == std::string::npos) {
                return false;
            }
            out = line.substr(q1 + 1, q2 - q1 - 1);
            return true;
        }

        bool HashRec(ShaderFileSystem                &fs,
                     const std::string               &path,
                     std::unordered_set<std::string> &visited,
                     uint64_t                        &hash,
                     std::string                     *rootContent,
                     ShaderFileSystem                &snapshot)
        {
            if (visited.count(path) != 0) {
                return true;
            }
            std::string content;
            if (!fs.ReadFile(path, content)) {
                return false;
            }
            visited.insert(path);
            if (!snapshot.IsBundlePath(path)) {
                snapshot.AddVirtualFile(path, content);
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
                bool              rootRelative = false;
                // This conservative scan includes inactive branches; packaged shaders may omit C++-only headers.
                if (ParseInclude(line, inc, rootRelative)) {
                    if ((rootRelative || !HashRec(fs, DirOf(path) + inc, visited, hash, nullptr, snapshot)) &&
                        !HashRec(fs, inc, visited, hash, nullptr, snapshot)) {
                        hash = HashMixBytes(hash, "unresolved:" + inc);
                    }
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

        bool CaptureSources(ShaderFileSystem                  &sourceFs,
                            const std::string                 &relativePath,
                            const std::vector<std::string>    &extraDeps,
                            uint64_t                          &outHash,
                            std::string                       &outSource,
                            std::shared_ptr<ShaderFileSystem> &snapshot)
        {
            snapshot = sourceFs.Snapshot(relativePath);
            if (sourceFs.IsBundlePath(relativePath)) {
                if (!snapshot->ReadFile(relativePath, outSource)) {
                    return false;
                }
                // Whole-bundle hashing also covers macro includes and Slang imports without a second preprocessor.
                outHash = HashMixBytes(snapshot->BundleFingerprint(), relativePath);
                return true;
            }
            auto                           &fs = sourceFs;
            std::unordered_set<std::string> visited;
            std::string                     root;
            uint64_t                        hash = kFnv1aBasis;
            if (!HashRec(fs, relativePath, visited, hash, &root, *snapshot)) {
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
    } // namespace

    ShaderResolver::ShaderResolver(
        sky::IFileSystem *offlineRoot, sky::IFileSystem *localRoot, ShaderFileSystem &sourceFs, uint64_t layoutFp, uint64_t, ShaderResolveMode mode)
        : mOffline(offlineRoot), mLocal(localRoot), mSourceFs(sourceFs), mLayoutFp(layoutFp), mMode(mode)
    {
    }
    bool ShaderResolver::HashSourceClosure(
        ShaderFileSystem &fs, const std::string &relativePath, const std::vector<std::string> &extraDeps, uint64_t &outHash, std::string &outSource)
    {
        std::shared_ptr<ShaderFileSystem> snapshot;
        return CaptureSources(fs, relativePath, extraDeps, outHash, outSource, snapshot);
    }
    bool ShaderResolver::SetAsyncCacheOwners(FileSystemPtr local, std::shared_ptr<ShaderCacheWriter> writer)
    {
        if (writer && writer->GetRoot() != local.Get())
            return false;
        mLocal      = local.Get();
        mWriter     = writer.get();
        mAsyncCache = std::make_shared<AsyncCacheOwners>(std::move(local), std::move(writer));
        return true;
    }
    bool ShaderResolver::ResolveSchema(
        const std::string &relativePath, uint32_t target, ShaderVariantSchema &outSchema, uint64_t &outSchemaFp, std::string *error)
    {
        if (mMode == ShaderResolveMode::DEVELOPMENT) {
            ShaderCompanion companion;
            if (LoadShaderCompanion(mSourceFs, relativePath, companion, nullptr)) {
                outSchema   = companion.schema;
                outSchemaFp = HashSchema(companion.schema);
                return true;
            }
        }
        for (auto *root : {mOffline, mMode == ShaderResolveMode::DEVELOPMENT ? mLocal : nullptr}) {
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
            if (!LoadIndex(*root, ShaderTargetDirName(target), index)) {
                continue;
            }
            const auto *entry = FindPathEntry(index, relativePath);
            if (entry != nullptr) {
                outSchema   = entry->schema;
                outSchemaFp = entry->schemaFp;
                return true;
            }
        }
        if (error != nullptr) {
            *error = "shader schema absent or requires recook: " + relativePath;
        }
        return false;
    }
    ShaderResolveStatus
    ShaderResolver::PrepareShader(const Request &request, ShaderCompileResult &outResult, ShaderCompileTask &outTask, std::string *error)
    {
        auto fail = [error](const std::string &message) {
            if (error != nullptr) {
                *error = message;
            }
            return ShaderResolveStatus::Failed;
        };
        if (request.target > static_cast<uint32_t>(ShaderTarget::DXIL) || request.relativePath.empty() || request.entry.empty()) {
            return fail("invalid shader request");
        }
        ShaderVariantSchema localSchema;
        const auto         *schema   = request.schema;
        uint64_t            schemaFp = request.schemaFp;
        if (schema == nullptr) {
            if (!ResolveSchema(request.relativePath, request.target, localSchema, schemaFp, error)) {
                return ShaderResolveStatus::Failed;
            }
            schema = &localSchema;
        }
        ShaderVariantSchema mergedSchema;
        if (request.pipelineSchema != nullptr) {
            mergedSchema = MergeVariantSchemas(*schema, request.pipelineSchema);
            schema       = &mergedSchema;
            schemaFp     = 0;
        }
        std::string validation;
        if (!schema->Validate(&validation)) {
            return fail(validation);
        }
        const auto actualSchemaFp = HashSchema(*schema);
        if (schemaFp != 0 && schemaFp != actualSchemaFp) {
            return fail("shader schema fingerprint mismatch");
        }
        schemaFp         = actualSchemaFp;
        auto environment = request.environment;
        if (environment == ShaderTargetEnvironment{}) {
            if (mMode == ShaderResolveMode::PRODUCTS) {
                if (mOffline == nullptr ||
                    !LoadShaderProductEnvironment(*mOffline, static_cast<ShaderTarget>(request.target), environment, validation)) {
                    return fail(validation.empty() ? "PRODUCTS shader resolver has no published root" : validation);
                }
            } else {
                environment = DefaultShaderEnvironment(static_cast<ShaderTarget>(request.target));
            }
        }
        if (environment.target != static_cast<ShaderTarget>(request.target) || !NormalizeShaderEnvironment(environment, validation)) {
            return fail(validation.empty() ? "shader request target/environment mismatch" : validation);
        }
        ShaderCacheKey key;
        key.layoutFp    = mLayoutFp;
        key.schemaFp    = schemaFp;
        key.target      = request.target;
        key.stage       = static_cast<uint32_t>(request.stage);
        key.entryHash   = HashEntryName(request.entry);
        key.sourcePath  = request.relativePath;
        key.sourceEntry = request.entry;
        key.environment = environment;
        if (!CanonicalShaderVariants(request.variant, *schema, environment.target, key.variants, error)) {
            return ShaderResolveStatus::Failed;
        }
        key.variantHash = HashShaderVariants(key.variants);
        if (ShaderUsageStore::Enabled()) {
            ShaderUsageEntry usage;
            usage.relativePath = request.relativePath;
            usage.entry        = request.entry;
            usage.stage        = static_cast<uint32_t>(request.stage);
            usage.target       = request.target;
            usage.variantHash  = request.variantHash != 0 ? request.variantHash : key.variantHash;
            usage.variantKey[0] = request.variantKey[0];
            usage.variantKey[1] = request.variantKey[1];
            usage.variantDump   = request.variantDump;
            usage.environment   = environment;
            if (request.variant != nullptr) {
                usage.variant = *request.variant;
            }
            ShaderUsageStore::Record(usage);
        }
        if (mMode == ShaderResolveMode::PRODUCTS) {
            if (mOffline == nullptr) {
                return fail("PRODUCTS shader resolver has no published root");
            }
            ShaderCacheIndex index;
            if (!LoadIndex(*mOffline, ShaderTargetDirName(request.target), index)) {
                return fail("published shader index absent, corrupt or requires recook");
            }
            const auto *entry = FindPathEntry(index, request.relativePath);
            const ShaderCacheArtifact *record = nullptr;
            if (entry)
                for (const auto &published : entry->artifacts) {
                    const auto &candidate = published.second;
                    if (candidate.key.sourceEntry == key.sourceEntry && candidate.key.stage == key.stage &&
                        candidate.key.schemaFp == key.schemaFp && candidate.key.layoutFp == key.layoutFp && candidate.key.environment == key.environment &&
                        std::ranges::find(candidate.selections, key.variants) != candidate.selections.end()) {
                        if (record && record->artifactId != candidate.artifactId)
                            return fail("Published selection maps to multiple stage artifacts");
                        record = &candidate;
                    }
                }
            if (record == nullptr) {
                return fail("published shader request/environment not found: " + request.relativePath + ":" + request.entry);
            }
            ShaderBlobStore store(mOffline, nullptr);
            if (!store.LoadArtifact(*record, outResult)) {
                return fail(store.LastError());
            }
            outResult.selection = key.variants;
            return ShaderResolveStatus::Ready;
        }
        const auto selection = key.variants;
        uint64_t                          sourceHash = 0;
        std::string                       source;
        std::shared_ptr<ShaderFileSystem> snapshot;
        if (!CaptureSources(mSourceFs, request.relativePath, request.depends, sourceHash, source, snapshot)) {
            return fail("DEVELOPMENT shader source not found: " + request.relativePath);
        }
        const auto owner    = ShaderCompilerFactory::Get().Acquire(true);
        auto      *compiler = owner ? owner->compiler.get() : nullptr;
        if (compiler == nullptr) {
            return fail("DEVELOPMENT shader resolution requires a compiler producer identity");
        }
        key.sourceHash = sourceHash;
        key.producer   = compiler->GetProducerIdentity(static_cast<ShaderTarget>(request.target));
        if (key.producer.compiler.empty() || key.producer.version.empty() || !key.producer.adapterVersion ||
            key.producer.reflectionVersion != ShaderReflection::kProtocolVersion) {
            return fail("DEVELOPMENT shader target has no valid compiler producer identity");
        }
        key.toolchainFp = ShaderProducerFingerprint(key.producer);
        ShaderCompileDesc inspection;
        inspection.source = source;
        inspection.sourcePath = request.relativePath;
        inspection.entry = request.entry;
        inspection.stage = request.stage;
        inspection.target = environment.target;
        inspection.environment = environment;
        inspection.schema = schema;
        inspection.variant = request.variant;
        inspection.fileSystem = snapshot.get();
        std::vector<ShaderVariantBinding> bindings;
        if (!compiler->InspectVariants(inspection, bindings, validation) ||
            !CanonicalShaderVariants(request.variant, *schema, environment.target, key.variants, &validation, &bindings))
            return fail(validation);
        key.variantHash = HashShaderVariants(key.variants);
        const auto publishSelection = [&]() {
            outResult.selection = selection;
            if (mWriter) {
                ShaderCacheWriter::Pending pending;
                pending.key = key;
                pending.result = outResult;
                pending.schema = *schema;
                pending.schemaFp = schemaFp;
                pending.sourceHash = sourceHash;
                pending.relativePath = request.relativePath;
                pending.deps = request.depends;
                return mWriter->Submit(std::move(pending), error);
            }
            if (!mLocal)
                return true;
            ShaderBlobStore store(mLocal, mLocal);
            return store.Store(key, outResult) && UpsertLocalArtifact(*mLocal, ShaderTargetDirName(key.target), request.relativePath, *schema, schemaFp,
                                                                    key, outResult, request.depends, error);
        };
        for (auto *root : {mOffline, mLocal}) {
            if (root == nullptr) {
                continue;
            }
            ShaderBlobStore store(root, nullptr);
            if (root == mLocal && mWriter != nullptr) {
                ShaderCacheArtifact record;
                if (mWriter->LookupArtifact(key, record) && store.LoadArtifact(record, outResult)) {
                    return publishSelection() ? ShaderResolveStatus::Ready : ShaderResolveStatus::Failed;
                }
            } else if (store.Load(key, outResult)) {
                return publishSelection() ? ShaderResolveStatus::Ready : ShaderResolveStatus::Failed;
            }
        }
        ShaderCompileTask task;
        task.key          = std::move(key);
        task.source       = std::move(source);
        task.entry        = request.entry;
        task.stage        = request.stage;
        task.target       = static_cast<ShaderTarget>(request.target);
        task.schema       = *schema;
        task.fileSystem   = std::move(snapshot);
        task.relativePath = request.relativePath;
        task.deps         = request.depends;
        task.schemaFp     = schemaFp;
        task.sourceHash   = sourceHash;
        // The compiler receives effective macro values; runtime specialization is applied by the pipeline.
        ShaderVariant compileVariant;
        for (const auto &entry : selection) {
            compileVariant.entries.push_back({Name(entry.name.c_str()), entry.value, entry.type});
        }
        task.variant = std::move(compileVariant);
        outTask      = std::move(task);
        return ShaderResolveStatus::Compile;
    }
    static bool CommitArtifact(
        sky::IFileSystem *local, ShaderCacheWriter *writer, const ShaderCompileTask &task, const ShaderCompileResult &result, std::string *error)
    {
        if (writer != nullptr) {
            ShaderCacheWriter::Pending pending;
            pending.key          = task.key;
            pending.result       = result;
            pending.schema       = task.schema;
            pending.schemaFp     = task.schemaFp;
            pending.sourceHash   = task.sourceHash;
            pending.relativePath = task.relativePath;
            pending.deps         = task.deps;
            return writer->Submit(std::move(pending), error);
        }
        if (local == nullptr) {
            return true;
        }
        ShaderBlobStore store(local, local);
        if (!store.Store(task.key, result)) {
            if (error != nullptr) {
                *error = store.LastError();
            }
            return false;
        }
        return UpsertLocalArtifact(*local, ShaderTargetDirName(task.key.target), task.relativePath, task.schema, task.schemaFp, task.key, result,
                                   task.deps, error);
    }
    bool ShaderResolver::CommitCompile(const ShaderCompileTask &task, const ShaderCompileResult &result, std::string *error)
    {
        if (mMode == ShaderResolveMode::PRODUCTS) {
            if (error != nullptr) {
                *error = "PRODUCTS shader resolver cannot publish compiles";
            }
            return false;
        }
        return CommitArtifact(mLocal, mWriter, task, result, error);
    }
    bool ShaderResolver::ResolveShader(const Request &request, ShaderCompileResult &out, std::string *error)
    {
        ShaderCompileTask task;
        const auto        status = PrepareShader(request, out, task, error);
        if (status != ShaderResolveStatus::Compile) {
            return status == ShaderResolveStatus::Ready;
        }
        const auto          owner    = ShaderCompilerFactory::Get().Acquire(true);
        auto               *compiler = owner ? owner->compiler.get() : nullptr;
        ShaderCompileResult result;
        if (compiler == nullptr || !task.Run(*compiler, result, error) || !CommitCompile(task, result, error)) {
            return false;
        }
        out = std::move(result);
        return true;
    }
    ShaderResolveStatus
    ShaderResolver::ResolveShaderAsync(const Request &request, ShaderCompileResult &outResult, ShaderCompileFuture &outFuture, std::string *error)
    {
        outFuture                  = {};
        const auto        compiler = ShaderCompilerFactory::Get().Acquire();
        ShaderCompileTask task;
        const auto        status = PrepareShader(request, outResult, task, error);
        if (status != ShaderResolveStatus::Compile) {
            return status;
        }
        if (!compiler || ((mLocal || mWriter) && (!mAsyncCache || mAsyncCache->root.Get() != mLocal || mAsyncCache->writer.get() != mWriter))) {
            if (error)
                *error = "Async shader compilation requires compiler and cache lifetime owners";
            return ShaderResolveStatus::Failed;
        }
        if (compiler->compiler->GetProducerIdentity(task.target) != task.key.producer) {
            if (error)
                *error = "Shader compiler registration changed while preparing an async task";
            return ShaderResolveStatus::Failed;
        }
        auto state      = std::make_shared<ShaderCompileFuture::State>();
        state->task     = std::move(task);
        state->compiler = compiler;
        state->commit   = [owners = mAsyncCache](const ShaderCompileTask &task, const ShaderCompileResult &result, std::string *error) {
            return CommitArtifact(owners ? owners->root.Get() : nullptr, owners ? owners->writer.get() : nullptr, task, result, error);
        };
        outFuture.mState = std::move(state);
        return ShaderResolveStatus::Compile;
    }
} // namespace sky::aurora
