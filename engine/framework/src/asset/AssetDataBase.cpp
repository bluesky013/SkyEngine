//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetDataBase.h>

#include "AssetBuildGuard.h"
#include "SourceManifestTransaction.h"

#include <core/file/FileUtil.h>
#include <core/file/MultiFileSystem.h>
#include <core/hash/Hash.h>
#include <core/logger/Logger.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_set>

static const char *TAG = "AssetDataBase";

namespace sky {

    namespace {
        AssetSourcePtr CopySource(const AssetSourcePtr &source)
        {
            AssetSourcePtr copy = new AssetSourceInfo;
            copy->revision      = source->revision;
            copy->uuid          = source->uuid;
            copy->path          = source->path;
            copy->name          = source->name;
            copy->mount         = source->mount;
            copy->ext           = source->ext;
            copy->dependencies  = source->dependencies;
            return copy;
        }

        AssetMutationResult CommitResult(const AssetFileWriteResult &write, const AssetSourcePtr &source = {})
        {
            AssetMutationResult result;
            result.status   = write.status;
            result.recovery = write.recovery;
            result.stage    = "commit-manifest";
            if (write) {
                result.state  = AssetCommitState::Committed;
                result.source = source;
            }
            return result;
        }
    } // namespace

    void AssetDataBase::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        std::lock_guard writeLock(*mutationMutex);
        const auto      next = fs ? CastPtr<NativeFileSystem>(fs->CreateSubSystem("assets", true)) : NativeFileSystemPtr{};
        std::lock_guard lock(assetMutex);
        for (const auto &[id, source] : idMap)
            source->revision->fetch_add(1, std::memory_order_acq_rel);
        engineFs = next;
        RebuildMounts();
    }

    void AssetDataBase::SetWorkSpaceFs(const FileSystemPtr &fs)
    {
        std::lock_guard writeLock(*mutationMutex);
        std::lock_guard lock(assetMutex);
        for (const auto &[id, source] : idMap)
            source->revision->fetch_add(1, std::memory_order_acq_rel);
        workSpaceFs = fs;
        RebuildMounts();
    }

    void AssetDataBase::RebuildMounts()
    {
        mountInfos.clear();
        mountFsList.clear();

        const auto addMount = [this](const std::string &id, const std::string &name, bool writable, const FileSystemPtr &fs) {
            if (fs == nullptr) {
                return;
            }
            mountInfos.push_back(AssetMount{id, name, writable});
            mountFsList.push_back(fs);
        };
        addMount("workspace", "Project", true, workSpaceFs);
        addMount("engine", "Engine", false, engineFs);

        auto mounts = new MultiFileSystem();
        for (const auto &fs : mountFsList) {
            mounts->AddFileSystem(fs);
        }
        mountFs = mounts;
    }

    int AssetDataBase::FindMountIndex(const FilePath &path) const
    {
        for (size_t i = 0; i < mountFsList.size(); ++i) {
            if (mountFsList[i] != nullptr && mountFsList[i]->FileExist(path)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int AssetDataBase::FindMountIndex(const std::string &mount) const
    {
        for (size_t i = 0; i < mountInfos.size(); ++i) {
            if (mountInfos[i].id == mount) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    FileSystemPtr AssetDataBase::ResolveOwningFs(const AssetSourcePtr &source) const
    {
        const int index = source ? FindMountIndex(source->mount) : -1;
        return index >= 0 ? mountFsList[index] : FileSystemPtr{};
    }

    Uuid AssetDataBase::CalculateUuidByPath(const FilePath &path)
    {
        return Uuid::CreateWithSeed(Fnv1a32(path.GetStr()));
    }

    Uuid AssetDataBase::CalculateLegacyUuid(const std::string &mount, const FilePath &path) const
    {
        // Reproduce the legacy identity: Uuid::CreateWithSeed(HashCombine32(bundle, Fnv1a32(path))).
        // Legacy SourceAssetBundle ordinals: INVALID=0, ENGINE=1, WORKSPACE=2; map the mount role.
        const uint32_t bundle = mount == "engine" ? 1u : 2u; // ENGINE : WORKSPACE
        uint32_t       hash   = 0;
        HashCombine32(hash, bundle);
        HashCombine32(hash, Fnv1a32(path.GetStr()));
        return Uuid::CreateWithSeed(hash);
    }

    FileScanReport AssetDataBase::MigrateLegacyIdentity()
    {
        std::lock_guard writeLock(*mutationMutex);
        // Seed legacy `(bundle, path)` UUIDs into every source manifest. Existing entries and any
        // `assets.db` rows win (idempotent); only directories containing assets get a manifest.
        migrationMode    = true;
        const auto error = RebuildCacheFromScan();
        migrationMode    = false;
        return error;
    }

    AssetSourceSnapshot AssetDataBase::CaptureSource(const Uuid &id) const
    {
        std::lock_guard lock(assetMutex);
        const auto      found = idMap.find(id);
        if (found == idMap.end())
            return {};
        return {CopySource(found->second), ResolveOwningFs(found->second)};
    }

    ProductStatus AssetDataBase::PrepareBuildInput(const Uuid &id, AssetBuildRequest &out)
    {
        std::lock_guard writeLock(*mutationMutex);
        const auto      snapshot = CaptureSource(id);
        if (!snapshot.source || !snapshot.filesystem)
            return {ProductError::NotFound, "Cook source is not registered on an available mount"};
        const auto               &source = snapshot.source;
        SourceManifestTransaction transaction(manifests, snapshot.filesystem, source->path.Parent(), SourceCancellation());
        const int                 mount    = FindMountIndex(source->mount);
        const bool                writable = mount >= 0 && mountInfos[mount].writable;
        if (const auto status = transaction.Open(writable); !status)
            return {status.error == FileOperationError::Busy ? ProductError::Busy : ProductError::IOError, status.message};
        const auto *entry = transaction.manifest.Find(source->path.FileName());
        if ((writable && !entry) || (entry && entry->id != id))
            return {ProductError::Stale, "Cook source manifest identity changed"};
        const auto           file = snapshot.filesystem->OpenFile(source->path);
        std::vector<uint8_t> bytes;
        if (!file || !file->ReadBin(bytes) || bytes.size() > std::numeric_limits<uint32_t>::max())
            return {ProductError::IOError, "Cook source could not be captured completely"};
        BinaryDataPtr data = new BinaryData(static_cast<uint32_t>(bytes.size()));
        if (!bytes.empty())
            std::memcpy(data->Data(), bytes.data(), bytes.size());
        auto guard                 = std::make_shared<SourcePublicationGuard>();
        guard->serialization       = mutationMutex;
        guard->filesystem          = snapshot.filesystem;
        guard->path                = source->path;
        guard->uuid                = source->uuid;
        guard->cook                = entry ? entry->extra : std::string{};
        guard->hash                = BuildInputHash(bytes);
        guard->revision            = source->revision;
        guard->expectedRevision    = source->revision->load(std::memory_order_acquire);
        guard->writable            = writable;
        guard->hadManifestIdentity = entry != nullptr;
        AssetBuildRequest request;
        request.assetInfo        = source;
        request.file             = new BuildInputFile(file->GetPath(), data);
        request.publication      = guard;
        request.cookJson         = guard->cook;
        request.sourceRevision   = guard->expectedRevision;
        request.inputFingerprint = source->uuid.ToString();
        AppendBuildKey(request.inputFingerprint, source->mount);
        AppendBuildKey(request.inputFingerprint, source->path.GetStr());
        AppendBuildKey(request.inputFingerprint, guard->cook);
        AppendBuildKey(request.inputFingerprint, std::to_string(guard->hash));
        out = std::move(request);
        return {};
    }

    AssetMutationResult AssetDataBase::WriteSource(const Uuid &id, std::string_view text)
    {
        std::lock_guard writeLock(*mutationMutex);
        const auto      source = FindSource(id);
        FileSystemPtr   filesystem;
        if (const auto status = WritableSource(source, filesystem); !status)
            return {status, "validate"};
        SourceManifestTransaction transaction(manifests, filesystem, source->path.Parent(), SourceCancellation());
        if (const auto status = transaction.Open(); !status)
            return {status, "open-manifest"};
        const auto *entry = transaction.manifest.Find(source->path.FileName());
        if (!entry || entry->id != id)
            return {FileOperationError::AlreadyExists, "Source write requires its unchanged manifest identity"};
        auto result  = CommitResult(SaveAtomic(filesystem, source->path, std::string(text)), source);
        result.stage = "replace-source";
        if (result)
            source->revision->fetch_add(1, std::memory_order_acq_rel);
        return result;
    }

    AssetSourcePtr AssetDataBase::FindSource(const Uuid &id) const
    {
        const auto iter = idMap.find(id);
        return iter != idMap.end() ? iter->second : AssetSourcePtr{};
    }

    AssetSourcePtr AssetDataBase::FindSource(const FilePath &path) const
    {
        for (const auto &mount : mountInfos)
            if (const auto source = FindSource(mount.id, path))
                return source;
        return {};
    }

    AssetSourcePtr AssetDataBase::FindSource(const std::string &mount, const FilePath &path) const
    {
        const auto bucket = pathMap.find(mount);
        if (bucket == pathMap.end())
            return {};
        const auto found = bucket->second.find(path);
        return found != bucket->second.end() ? FindSource(found->second) : AssetSourcePtr{};
    }

    AssetSourcePtr AssetDataBase::FindAsset(const Uuid &id)
    {
        std::lock_guard lock(assetMutex);
        return FindSource(id);
    }

    AssetSourcePtr AssetDataBase::FindAsset(const FilePath &path)
    {
        std::lock_guard lock(assetMutex);
        return FindSource(path);
    }

    AssetSourcePtr AssetDataBase::FindAsset(const std::string &mount, const FilePath &path)
    {
        std::lock_guard lock(assetMutex);
        return FindSource(mount, path);
    }

    void AssetDataBase::InvalidatePath(const FilePath &path)
    {
        for (const auto &[mount, paths] : pathMap) {
            const auto found = paths.find(path);
            if (found == paths.end())
                continue;
            const auto source = idMap.find(found->second);
            if (source != idMap.end())
                source->second->revision->fetch_add(1, std::memory_order_acq_rel);
        }
    }

    std::function<bool()> AssetDataBase::SourceCancellation() const
    {
        const auto epoch = cancellationEpoch.load(std::memory_order_acquire);
        return [this, epoch] { return cancellationEpoch.load(std::memory_order_acquire) != epoch; };
    }

    std::vector<AssetSourcePtr> AssetDataBase::Gather(const std::string_view &category)
    {
        std::vector<AssetSourcePtr> res;

        std::lock_guard lock(assetMutex);
        for (auto &[id, asset] : idMap) {
            if (QueryType(asset->ext) == category) {
                res.emplace_back(CopySource(asset));
            }
        }

        return res;
    }

    std::unordered_map<Uuid, AssetSourcePtr> AssetDataBase::GetSources() const
    {
        std::lock_guard                          lock(assetMutex);
        std::unordered_map<Uuid, AssetSourcePtr> snapshot;
        snapshot.reserve(idMap.size());
        for (const auto &[id, asset] : idMap)
            snapshot.emplace(id, CopySource(asset));
        return snapshot;
    }

    void AssetDataBase::ForEachSource(const std::function<void(const AssetSourcePtr &)> &fn) const
    {
        if (!fn) {
            return;
        }

        const auto snapshot = GetSources();
        for (const auto &[id, asset] : snapshot) {
            fn(asset);
        }
    }

    std::string AssetDataBase::QueryType(const std::string &ext) const
    {
        auto *builder = AssetBuilderManager::Get()->QueryBuilder(ext);
        return builder != nullptr ? std::string(builder->QueryType(ext)) : std::string{};
    }

    void AssetDataBase::BuildDependencyGraph(AssetDependencyGraph &graph) const
    {
        graph.Clear();

        std::lock_guard lock(assetMutex);
        for (const auto &[id, info] : idMap) {
            graph.Add(id, info->dependencies);
        }
        for (const auto &[id, record] : generatedAssets) {
            if (record.active && idMap.contains(record.owner)) {
                graph.Add(id, {record.owner});
            }
        }
    }

    bool AssetDataBase::ResolvePath(const std::string &path, Uuid &out) const
    {
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(path);
        if (src == nullptr) {
            return false;
        }
        out = src->uuid;
        return true;
    }

    bool AssetDataBase::Exists(const Uuid &id) const
    {
        Uuid owner;
        return GetOwner(id, owner);
    }

    bool AssetDataBase::GetType(const Uuid &id, std::string &out) const
    {
        GeneratedAssetRecord child;
        if (GetGenerated(id, child)) {
            if (!child.active || !const_cast<AssetDataBase *>(this)->FindAsset(child.owner)) {
                return false;
            }
            out = child.type;
            return true;
        }
        const auto snapshot = CaptureSource(id);
        if (!snapshot.source)
            return false;
        out = QueryType(snapshot.source->ext);
        return !out.empty();
    }

    bool AssetDataBase::GetTarget(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        const auto snapshot = CaptureSource(owner);
        if (!snapshot.source)
            return false;
        out = AssetBuilderManager::Get()->GetCookConfig().ResolveTarget(CookJsonFor(snapshot));
        return true;
    }

    bool AssetDataBase::GetSourcePath(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        const auto snapshot = CaptureSource(owner);
        if (!snapshot.source)
            return false;
        out = snapshot.source->path.GetStr();
        return true;
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const FilePath &path, bool build)
    {
        std::vector<std::pair<std::string, FileSystemPtr>> mounts;
        {
            std::lock_guard lock(assetMutex);
            for (size_t i = 0; i < mountInfos.size(); ++i)
                mounts.emplace_back(mountInfos[i].id, mountFsList[i]);
        }
        for (const auto &[mount, filesystem] : mounts)
            if (filesystem && filesystem->FileExist(path))
                return RegisterAsset(mount, path, build);
        return {};
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const std::string &mount, const FilePath &path, bool build)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        const int       index = FindMountIndex(mount);
        if (index < 0 || !mountFsList[index] || !mountFsList[index]->FileExist(path)) {
            LOG_E(TAG, "Source is missing from mount %s: %s", mount.c_str(), path.GetStr().c_str());
            return {};
        }
        const auto  fs        = mountFsList[index];
        const bool                writable  = mountInfos[index].writable;
        const auto  directory = path.Parent();
        SourceManifestTransaction transaction(manifests, fs, directory, cancelled);
        if (const auto status = transaction.Open(writable); !status) {
            LOG_E(TAG, "%s: %s", path.GetStr().c_str(), status.message.c_str());
            return {};
        }
        auto &manifest = transaction.manifest;
        manifests.Invalidate(fs, directory);
        const auto *entry = manifest.Find(path.FileName());
        auto        info  = FindSource(mount, path);
        if (info && entry && entry->id != info->uuid) {
            LOG_E(TAG, "Source manifest identity changed: %s", path.GetStr().c_str());
            return {};
        }
        const Uuid uuid     = entry           ? entry->id
                              : info          ? info->uuid
                              : migrationMode ? CalculateLegacyUuid(mount, path)
                              : writable      ? Uuid::Create()
                                              : CalculateUuidByPath(path);
        const auto existing = idMap.find(uuid);
        if ((existing != idMap.end() && (existing->second->mount != mount || existing->second->path != path)) || generatedAssets.contains(uuid)) {
            LOG_E(TAG, "Source UUID belongs to another asset: %s", path.GetStr().c_str());
            return {};
        }
        if (!entry && writable) {
            IndexFileEntry added;
            added.key = path.FileName();
            added.id  = uuid;
            manifest.Set(added);
            if (const auto saved = transaction.Commit(manifest); !saved) {
                LOG_E(TAG, "Cannot persist source identity: %s: %s", path.GetStr().c_str(), saved.status.message.c_str());
                for (const auto &issue : saved.recovery)
                    LOG_E(TAG, "%s at %s: %s", issue.stage.c_str(), issue.path.GetStr().c_str(), issue.status.message.c_str());
                return {};
            }
        }
        transaction.Unlock();
        std::unique_lock publish(assetMutex);
        if (!info) {
            info        = new AssetSourceInfo();
            info->path  = path;
            info->uuid  = uuid;
            info->ext   = path.Extension();
            info->mount = mount;
            InvalidatePath(path);
            idMap.emplace(uuid, info);
            pathMap[mount].emplace(path, uuid);
        }
        entry = manifest.Find(path.FileName());
        if ((entry && !entry->generated.empty()) || generatedManifests.contains(info->uuid)) {
            GeneratedAssetManifest generated;
            const auto             parsed = ParseGeneratedAssets(info->uuid, entry ? entry->generated : std::string{}, generated);
            if (!parsed || !PublishGeneratedCatalog(info->uuid, generated))
                LOG_E(TAG, "Generated catalog refresh failed for %s", info->uuid.ToString().c_str());
        }
        if (build) {
            const auto id = info->uuid;
            publish.unlock();
            writeLock.unlock();
            AssetBuildRequest request;
            auto             *builder = AssetBuilderManager::Get();
            if (const auto status = builder->PrepareRequest(id, {}, request, this); status)
                builder->BuildRequest(request);
            else
                LOG_E(TAG, "Cannot prepare cook input: %s", status.Describe().c_str());
        }
        return info;
    }

    AssetMutationResult AssetDataBase::PersistSourceIdentity(const Uuid &id)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        const auto       source    = FindSource(id);
        FileSystemPtr    filesystem;
        if (const auto status = WritableSource(source, filesystem); !status)
            return {status, "validate"};
        SourceManifestTransaction transaction(manifests, filesystem, source->path.Parent(), cancelled);
        if (const auto status = transaction.Open(); !status)
            return {status, "open-manifest"};
        auto       &manifest = transaction.manifest;
        const auto *entry = manifest.Find(source->path.FileName());
        if (entry && entry->id != id)
            return {FileOperationError::AlreadyExists, "Source manifest belongs to a different identity"};
        if (!entry)
            manifest.Set({source->path.FileName(), id, {}, {}});
        return CommitResult(transaction.Commit(manifest), source);
    }

    FileOperationStatus AssetDataBase::WritableSource(const AssetSourcePtr &source, FileSystemPtr &out) const
    {
        if (!source)
            return {FileOperationError::InvalidPath, "Source asset is not registered"};
        for (size_t i = 0; i < mountInfos.size(); ++i) {
            if (mountInfos[i].id == source->mount) {
                if (!mountInfos[i].writable || !mountFsList[i] || mountFsList[i]->IsReadOnly())
                    return {FileOperationError::InvalidPath, "Source asset belongs to a read-only mount"};
                out = mountFsList[i];
                return {};
            }
        }
        return {FileOperationError::InvalidPath, "Source asset has no owning mount"};
    }

    AssetMutationResult AssetDataBase::RemoveAsset(const Uuid &id)
    {
        return RemoveSource(id, false);
    }

    AssetMutationResult AssetDataBase::DeleteAsset(const Uuid &id)
    {
        return RemoveSource(id, true);
    }

    void AssetDataBase::UnpublishSource(const AssetSourcePtr &source)
    {
        std::lock_guard publish(assetMutex);
        source->revision->fetch_add(1, std::memory_order_acq_rel);
        InvalidatePath(source->path);
        pathMap[source->mount].erase(source->path);
        idMap.erase(source->uuid);
        for (auto &[childId, record] : generatedAssets)
            if (record.owner == source->uuid)
                record.active = false;
        generatedManifests.erase(source->uuid);
    }

    AssetMutationResult AssetDataBase::RemoveSource(const Uuid &id, bool deleteFile)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        const auto       source    = FindSource(id);
        FileSystemPtr   fs;
        if (const auto status = WritableSource(source, fs); !status)
            return {status, "validate"};
        const auto                path = source->path;
        SourceManifestTransaction transaction(manifests, fs, path.Parent(), cancelled);
        if (const auto status = transaction.Open(); !status)
            return {status, "open-manifest"};
        auto       &manifest = transaction.manifest;
        const auto *entry    = manifest.Find(path.FileName());
        if (!entry || entry->id != id)
            return {FileOperationError::AlreadyExists, "Source manifest identity is missing or belongs to another asset"};
        const FilePath temporary(path.GetStr() + ".deleted-" + Uuid::Create().ToString());
        if (deleteFile && !fs->Rename(path, temporary))
            return {{FileOperationError::IOError, "Could not stage source deletion"}, "stage-delete"};
        manifest.Remove(path.FileName());
        auto result = CommitResult(transaction.Commit(manifest));
        if (!result) {
            if (deleteFile && !fs->Rename(temporary, path)) {
                result.state = AssetCommitState::RollbackFailed;
                result.recovery.push_back({"restore-source",
                                           fs->GetPath() / temporary,
                                           {FileOperationError::IOError, "Source bytes remain staged; restore the original manifest path"}});
                transaction.Unlock();
                UnpublishSource(source);
            }
            return result;
        }
        // The manifest is the commit point. A failed garbage-file removal cannot resurrect the deleted identity.
        transaction.Unlock();
        UnpublishSource(source);
        if (deleteFile) {
            if (const auto cleanup = fs->RemoveFile(temporary); !cleanup) {
                result.state  = AssetCommitState::CommittedNeedsRepair;
                result.status = cleanup;
                result.stage  = "remove-staged-source";
                result.recovery.push_back({result.stage, fs->GetPath() / temporary, cleanup});
            }
        }
        return result;
    }

    AssetMutationResult AssetDataBase::MoveAsset(const FilePath &from, const FilePath &to)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        const auto       source    = FindSource(from);
        FileSystemPtr   fs;
        if (const auto status = WritableSource(source, fs); !status)
            return {status, "validate"};
        AssetDestination destinationPath;
        if (const auto status = ResolveWritableDestination(to, destinationPath, source->mount); !status)
            return {status, "validate"};
        if (FindSource(source->mount, to) || fs->FileExist(to))
            return {FileOperationError::AlreadyExists, "Move destination is occupied"};
        const bool                sameDirectory = from.Parent() == to.Parent();
        SourceManifestTransaction origin(manifests, fs, from.Parent(), cancelled);
        SourceManifestTransaction destination(manifests, fs, to.Parent(), cancelled);
        auto                     *first  = &origin;
        auto                     *second = &destination;
        if (!sameDirectory && to.Parent().GetStr() < from.Parent().GetStr())
            std::swap(first, second);
        if (const auto status = first->Open(); !status)
            return {status, "open-manifest"};
        if (!sameDirectory) {
            if (const auto status = second->Open(); !status)
                return {status, "open-manifest"};
        }
        const auto &original = origin.manifest;
        const auto &target   = sameDirectory ? original : destination.manifest;
        const auto *entry = original.Find(from.FileName());
        if (!fs->FileExist(from) || fs->FileExist(to) || !entry || entry->id != source->uuid || target.Find(to.FileName()))
            return {FileOperationError::AlreadyExists, "Move source or destination identity changed"};
        GeneratedAssetManifest children;
        if (const auto status = ParseGeneratedAssets(source->uuid, entry->generated, children); !status)
            return {FileOperationError::InvalidPath, status.message};
        if (const auto status = CheckGeneratedCatalogConflicts(source->uuid, children); !status)
            return {FileOperationError::AlreadyExists, status.message};
        auto moved   = *entry;
        moved.key    = to.FileName();
        auto updated = original;
        updated.Remove(from.FileName());
        auto next = sameDirectory ? updated : target;
        next.Set(moved);
        if (!fs->Rename(from, to))
            return {{FileOperationError::IOError, "Move source rename failed"}, "move-source"};
        auto      &targetTransaction = sameDirectory ? origin : destination;
        const auto destinationWrite  = targetTransaction.Commit(next);
        auto       result            = CommitResult(destinationWrite);
        if (destinationWrite && !sameDirectory)
            result = CommitResult(origin.Commit(updated));
        if (!result) {
            bool restored = true;
            if (destinationWrite && !sameDirectory) {
                const auto rollback = destination.Commit(target);
                result.recovery.insert(result.recovery.end(), rollback.recovery.begin(), rollback.recovery.end());
                if (!rollback) {
                    restored = false;
                    result.recovery.push_back(
                        {"restore-destination-manifest", fs->GetPath() / to.Parent() / FilePath("assets.jsonl"), rollback.status});
                }
            }
            if (restored && !fs->Rename(to, from)) {
                restored = false;
                result.recovery.push_back({"restore-source", fs->GetPath() / to, {FileOperationError::IOError, "Source rename compensation failed"}});
            }
            if (!restored) {
                result.state = AssetCommitState::RollbackFailed;
                result.recovery.push_back({"recover-move",
                                           fs->GetPath() / to,
                                           {FileOperationError::IOError, "Reconcile source and destination manifests before restoring this source"}});
                origin.Unlock();
                destination.Unlock();
                UnpublishSource(source);
            }
            return result;
        }
        origin.Unlock();
        destination.Unlock();
        std::lock_guard publish(assetMutex);
        InvalidatePath(from);
        InvalidatePath(to);
        pathMap[source->mount].erase(from);
        source->revision->fetch_add(1, std::memory_order_acq_rel);
        source->path = to;
        pathMap[source->mount].emplace(to, source->uuid);
        result.source = source;
        if (const auto status = PublishGeneratedCatalog(source->uuid, children); !status) {
            result.state  = AssetCommitState::CommittedNeedsRepair;
            result.status = {FileOperationError::IOError, status.message};
            result.stage  = "publish-catalog";
        }
        return result;
    }

    FileOperationStatus AssetDataBase::ResolveWritableDestination(const FilePath &path, AssetDestination &out, const std::string &mount) const
    {
        std::lock_guard             lock(*mutationMutex);
        const auto                  text = path.GetStr();
        const std::filesystem::path logical(text);
        if (text.empty() || logical.is_absolute() || logical.has_root_name() || logical.lexically_normal().generic_string() != text ||
            text.find_first_of("\\:") != std::string::npos || text.find('\0') != std::string::npos || logical.filename().empty() ||
            std::any_of(logical.begin(), logical.end(), [](const auto &part) { return part == ".." || part == "."; }))
            return {FileOperationError::InvalidPath, "Destination must be a canonical mount-relative file path"};
        for (size_t i = 0; i < mountInfos.size(); ++i) {
            if (mountInfos[i].writable && (mount.empty() || mountInfos[i].id == mount) && mountFsList[i] && !mountFsList[i]->IsReadOnly()) {
                out = {mountInfos[i].id, mountFsList[i], path};
                return {};
            }
        }
        return {FileOperationError::InvalidPath, "Writable destination mount is unavailable"};
    }

    AssetMutationResult AssetDataBase::CreateSource(const AssetDestination       &destination,
                                                    std::span<const uint8_t>      bytes,
                                                    const std::string            &cook,
                                                    const GeneratedAssetManifest &generated)
    {
        std::unique_lock    writeLock(*mutationMutex);
        const auto          cancelled = SourceCancellation();
        AssetMutationResult result;
        const auto         &fs    = destination.filesystem;
        const auto         &path  = destination.path;
        const int           mount = FindMountIndex(destination.mount);
        if (mount < 0 || !mountInfos[mount].writable || mountFsList[mount].Get() != fs.Get()) {
            result.status = {FileOperationError::InvalidPath, "Destination mount changed before source creation"};
            return result;
        }
        SourceManifestTransaction transaction(manifests, fs, path.Parent(), cancelled);
        result.stage  = "open-manifest";
        result.status = transaction.Open();
        if (!result.status)
            return result;
        auto &manifest = transaction.manifest;
        if (manifest.Find(path.FileName()) || FindSource(destination.mount, path) || fs->FileExist(path)) {
            result.status = {FileOperationError::AlreadyExists, "Destination file or identity already exists"};
            return result;
        }
        const auto                            id = Uuid::Create();
        std::vector<GeneratedAssetDescriptor> requested;
        for (const auto &record : generated.records)
            if (record.active)
                requested.push_back({record.key, record.type});
        GeneratedAssetManifest children;
        if (const auto status = ReconcileGeneratedAssets(id, {}, requested, generated.defaults, children); !status) {
            result.status = {FileOperationError::InvalidPath, status.message};
            return result;
        }
        if (const auto status = CheckGeneratedCatalogConflicts(id, children); !status) {
            result.status = {FileOperationError::AlreadyExists, status.message};
            return result;
        }
        manifest.Set(
            {path.FileName(), id, cook, requested.empty() && generated.defaults.empty() ? std::string{} : SerializeGeneratedAssets(children)});
        result.stage  = "write-source";
        result.status = fs->WriteFileExclusive(path, bytes);
        if (!result.status) {
            if (result.status.error != FileOperationError::AlreadyExists && fs->FileExist(path)) {
                result.recovery.push_back({"write-source",
                                           fs->GetPath() / path,
                                           {FileOperationError::IOError, "Failed exclusive write left a file; verify ownership before removal"}});
            }
            return result;
        }
        result = CommitResult(transaction.Commit(manifest));
        if (!result) {
            if (const auto cleanup = fs->RemoveFile(path); !cleanup) {
                result.state = AssetCommitState::RollbackFailed;
                result.recovery.push_back({"remove-source", fs->GetPath() / path, cleanup});
            }
            return result;
        }
        transaction.Unlock();
        std::lock_guard publish(assetMutex);
        result.source        = new AssetSourceInfo;
        result.source->uuid  = id;
        result.source->path  = path;
        result.source->mount = destination.mount;
        result.source->ext   = path.Extension();
        InvalidatePath(path);
        idMap.emplace(id, result.source);
        pathMap[destination.mount].emplace(path, id);
        result.state = AssetCommitState::Committed;
        if (!children.records.empty() || !children.defaults.empty()) {
            if (const auto status = PublishGeneratedCatalog(id, children); !status) {
                result.state  = AssetCommitState::CommittedNeedsRepair;
                result.status = {FileOperationError::IOError, "Source committed; generated catalog requires repair: " + status.message};
                result.stage  = "publish-catalog";
            }
        }
        return result;
    }

    AssetMutationResult AssetDataBase::CreateAsset(const std::string &mount, const FilePath &path, std::span<const uint8_t> bytes)
    {
        AssetDestination destination;
        if (const auto status = ResolveWritableDestination(path, destination, mount); !status)
            return {status, "validate"};
        return CreateSource(destination, bytes, {}, {});
    }

    AssetMutationResult AssetDataBase::DuplicateAsset(const FilePath &from, const FilePath &to)
    {
        std::unique_lock    writeLock(*mutationMutex);
        const auto          cancelled = SourceCancellation();
        AssetMutationResult result;
        AssetDestination    destination;
        result.status = ResolveWritableDestination(to, destination);
        if (!result.status)
            return result;
        const auto           original = FindSource(from);
        const auto           fs       = ResolveOwningFs(original);
        const auto           input    = fs ? fs->OpenFile(from) : FilePtr{};
        std::vector<uint8_t> bytes;
        if (!original || !input || !input->ReadBin(bytes)) {
            result.status = {FileOperationError::IOError, "Duplicate source could not be read"};
            return result;
        }
        const auto  snapshot = AssetIndexFile::Load(fs, from.Parent() / FilePath("assets.jsonl"), "file", "cook");
        const auto *entry    = snapshot.Find(from.FileName());
        if (!snapshot.IsComplete() || !entry || entry->id != original->uuid) {
            result.status = {FileOperationError::InvalidPath, "Duplicate source manifest is damaged or its identity changed"};
            return result;
        }
        GeneratedAssetManifest children;
        if (const auto status = ParseGeneratedAssets(original->uuid, entry->generated, children); !status) {
            result.status = {FileOperationError::InvalidPath, status.message};
            return result;
        }
        return CreateSource(destination, bytes, entry->extra, children);
    }

    AssetMutationResult AssetDataBase::ImportAsset(const FilePath &sourceFile, const FilePath &dest, bool cook)
    {
        AssetMutationResult result;
        AssetDestination    destination;
        result.status = ResolveWritableDestination(dest, destination);
        if (!result.status)
            return result;
        auto input = sourceFile.OpenIFStream(std::ios::binary);
        if (!input) {
            result.status = {FileOperationError::IOError, "Import source could not be opened"};
            return result;
        }
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (input.bad()) {
            result.status = {FileOperationError::IOError, "Import source could not be read completely"};
            return result;
        }
        input.close();
        result = CreateSource(destination, bytes, {}, {});
        if (result.state == AssetCommitState::Committed && cook)
            BuildAllTargets(result.source->uuid);
        return result;
    }

    FilePtr AssetDataBase::OpenFile(const AssetSourcePtr &src)
    {
        const auto snapshot = src ? CaptureSource(src->uuid) : AssetSourceSnapshot{};
        return snapshot.source && snapshot.filesystem ? snapshot.filesystem->OpenFile(snapshot.source->path) : FilePtr{};
    }

    FilePtr AssetDataBase::OpenFile(const FilePath &path)
    {
        FileSystemPtr filesystem;
        {
            std::lock_guard lock(assetMutex);
            filesystem = mountFs;
        }
        return filesystem ? filesystem->OpenFile(path) : FilePtr{};
    }

    FilePtr AssetDataBase::CreateOrOpenFile(const FilePath &path)
    {
        FileSystemPtr filesystem;
        {
            std::lock_guard lock(assetMutex);
            filesystem = mountFs;
        }
        return filesystem ? filesystem->CreateOrOpenFile(path) : FilePtr{};
    }

    void AssetDataBase::SetBuildDependencies(const Uuid &id, const std::vector<Uuid> &dependencies)
    {
        std::lock_guard writeLock(*mutationMutex);
        std::lock_guard lock(assetMutex);
        const auto      entry = idMap.find(id);
        if (entry != idMap.end()) {
            entry->second->dependencies = dependencies;
        }
    }

    bool AssetDataBase::SetBuildDependencies(const AssetBuildRequest &request)
    {
        if (!request.assetInfo)
            return false;
        std::lock_guard writeLock(*mutationMutex);
        std::lock_guard lock(assetMutex);
        const auto      current = FindSource(request.assetInfo->uuid);
        if (!current || current->mount != request.assetInfo->mount || current->path != request.assetInfo->path ||
            (request.publication &&
             (current->revision != request.assetInfo->revision || current->revision->load(std::memory_order_acquire) != request.sourceRevision)))
            return false;
        current->dependencies = request.assetInfo->dependencies;
        return true;
    }

    void AssetDataBase::SetMarkedName(const Uuid &id, const std::string &name)
    {
        std::lock_guard writeLock(*mutationMutex);
        std::lock_guard lock(assetMutex);
        auto iter = idMap.find(id);
        if (iter == idMap.end()) {
            return;
        }

        if (iter->second->name != name)
            iter->second->revision->fetch_add(1, std::memory_order_acq_rel);
        iter->second->name = name;
    }

    FileScanReport AssetDataBase::Load()
    {
        std::lock_guard writeLock(*mutationMutex);
        FileScanReport  cacheReport;
        manifests.Clear();
        const auto scanned = ScanSources();
        if (const auto *error = std::get_if<FileScanError>(&scanned)) {
            return {{*error}, {}, {}};
        }
        auto file = workSpaceFs ? workSpaceFs->OpenFile("assets.db") : FilePtr{};
        if (file) {
            auto             archive = file->ReadAsArchive();
            JsonInputArchive json(*archive);

            {
                uint32_t count = json.StartArray("assets");
                for (uint32_t i = 0; i < count; ++i) {
                    AssetSourcePtr pInfo = new AssetSourceInfo();
                    auto          &info  = *pInfo;

                    json.Start("uuid");
                    info.uuid = Uuid::CreateFromString(json.LoadString());
                    json.End();

                    json.Start("markedName");
                    info.name = json.LoadString();
                    json.End();

                    json.Start("ext");
                    info.ext = json.LoadString();
                    json.End();

                    json.Start("path");
                    info.path = json.LoadString();
                    json.End();

                    if (json.Start("mount")) {
                        info.mount = json.LoadString();
                        json.End();
                    }

                    uint32_t depCount = json.StartArray("dependencies");
                    for (uint32_t j = 0; j < depCount; ++j) {
                        info.dependencies.emplace_back(Uuid::CreateFromString(json.LoadString()));
                        json.NextArrayElement();
                    }
                    json.End();

                    json.NextArrayElement();

                    if (info.mount.empty()) {
                        for (size_t index = 0; index < mountFsList.size(); ++index) {
                            const auto &fs = mountFsList[index];
                            const auto  snapshot = manifests.Get(fs, info.path.Parent());
                            const auto *entry    = snapshot->Find(info.path.FileName());
                            if (entry && entry->id == info.uuid && fs->FileExist(info.path)) {
                                info.mount = mountInfos[index].id;
                                break;
                            }
                        }
                        if (info.mount.empty()) {
                            const int index = FindMountIndex(info.path);
                            if (index >= 0) {
                                info.mount = mountInfos[index].id;
                            }
                        }
                    }
                    const auto fs = ResolveOwningFs(pInfo);
                    if (!fs || !fs->FileExist(info.path)) {
                        continue;
                    }
                    const auto  snapshot = manifests.Get(fs, info.path.Parent());
                    const auto *entry    = snapshot->Find(info.path.FileName());
                    if (entry && entry->id != info.uuid)
                        cacheReport.warnings.push_back({info.mount, info.path, 0, "Manifest identity supersedes assets.db identity"});
                    if (!info.uuid || !snapshot->IsComplete() || (entry && entry->id != info.uuid) || idMap.contains(info.uuid) ||
                        generatedAssets.contains(info.uuid)) {
                        continue;
                    }
                    std::lock_guard publish(assetMutex);
                    pathMap[info.mount].emplace(info.path, info.uuid);
                    InvalidatePath(info.path);
                    idMap.emplace(info.uuid, pInfo);
                }

                json.End();
            }
        }

        auto report = RegisterScannedSources(std::get<std::vector<MountedPath>>(scanned));
        for (const auto &warning : cacheReport.warnings)
            LOG_W(TAG, "%s:%s: %s", warning.mount.c_str(), warning.path.GetStr().c_str(), warning.message.c_str());
        report.warnings.insert(report.warnings.end(), cacheReport.warnings.begin(), cacheReport.warnings.end());
        return report;
    }

    void AssetDataBase::Save()
    {
        AssetExecutor::Get()->WaitForAll();
        std::lock_guard writeLock(*mutationMutex);
        const auto      sources = GetSources();

        if (workSpaceFs == nullptr) {
            return;
        }

        auto              file    = workSpaceFs->CreateOrOpenFile("assets.db");
        auto              archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);

        {

            json.StartObject();
            json.Key("assets");
            json.StartArray();
            for (const auto &[id, pInfo] : sources) {
                auto &info = *pInfo;
                json.StartObject();

                // info
                json.Key("uuid");
                json.SaveValue(info.uuid.ToString());

                json.Key("markedName");
                json.SaveValue(info.name);

                json.Key("ext");
                json.SaveValue(info.ext);

                json.Key("path");
                json.SaveValue(info.path.GetStr());

                json.Key("mount");
                json.SaveValue(info.mount);

                json.Key("dependencies");
                json.StartArray();

                for (auto &dep : info.dependencies) {
                    json.SaveValue(dep.ToString());
                }
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();
        }
    }

    void AssetDataBase::Reset()
    {
        cancellationEpoch.fetch_add(1, std::memory_order_acq_rel);
        std::lock_guard writeLock(*mutationMutex);
        {
            std::lock_guard lock(assetMutex);
            for (const auto &[id, source] : idMap)
                source->revision->fetch_add(1, std::memory_order_acq_rel);
            pathMap.clear();
            idMap.clear();
            generatedAssets.clear();
            generatedManifests.clear();
        }
        manifests.Clear();
    }

    FileScanReport AssetDataBase::RebuildCacheFromScan()
    {
        std::lock_guard writeLock(*mutationMutex);
        manifests.Clear();
        const auto scanned = ScanSources();
        if (const auto *error = std::get_if<FileScanError>(&scanned)) {
            return {{*error}, {}, {}};
        }
        return RegisterScannedSources(std::get<std::vector<MountedPath>>(scanned));
    }

    FileScanReport AssetDataBase::RegisterScannedSources(const std::vector<MountedPath> &sources)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        FileScanReport   report;
        struct Directory {
            std::string           mount;
            FilePath              path;
            FileSystemPtr         filesystem;
            bool                  writable = false;
            bool                  blocked  = false;
            AssetIndexFile        manifest{"file", "cook"};
            std::vector<FilePath> sources;
        };
        std::map<std::pair<std::string, std::string>, Directory> directories;
        for (const auto &[mount, path] : sources) {
            const int index = FindMountIndex(mount);
            if (index < 0 || !mountFsList[index]) {
                report.errors.push_back({path, std::make_error_code(std::errc::no_such_device)});
                continue;
            }
            auto &directory      = directories[{mount, path.Parent().GetStr()}];
            directory.mount      = mount;
            directory.path       = path.Parent();
            directory.filesystem = mountFsList[index];
            directory.writable   = mountInfos[index].writable;
            directory.sources.push_back(path);
        }
        const auto block = [&](Directory &directory, const std::string &reason, bool recoverable = false) {
            if (!directory.blocked)
                report.blockedDirs.push_back({directory.mount, directory.path, reason, recoverable});
            directory.blocked = true;
        };
        std::unordered_map<Uuid, Directory *> owners;
        std::unordered_set<Uuid>              conflicts;
        for (auto &[key, directory] : directories) {
            directory.manifest = *manifests.Get(directory.filesystem, directory.path);
            for (const auto &error : directory.manifest.Errors()) {
                report.warnings.push_back({directory.mount, directory.path / FilePath("assets.jsonl"), error.line, error.message});
                block(directory, error.message);
            }
            for (const auto &entry : directory.manifest.Entries()) {
                const auto [previous, inserted] = owners.emplace(entry.id, &directory);
                if (!inserted) {
                    conflicts.insert(entry.id);
                    block(directory, "Source UUID is duplicated across directories");
                    block(*previous->second, "Source UUID is duplicated across directories");
                }
                GeneratedAssetManifest generated;
                if (const auto status = ParseGeneratedAssets(entry.id, entry.generated, generated); !status) {
                    block(directory, status.message);
                    report.warnings.push_back({directory.mount, directory.path / FilePath(entry.key), 0, status.message});
                }
            }
        }
        for (const auto &[id, owner] : owners) {
            if (conflicts.contains(id))
                continue;
            const auto cached = idMap.find(id);
            if (cached == idMap.end())
                continue;
            const auto  info  = cached->second;
            const auto *entry = owner->manifest.Find(info->path.FileName());
            if (info->mount == owner->mount && info->path.Parent() == owner->path && entry && entry->id == id)
                continue;
            report.warnings.push_back({info->mount, info->path, 0, "Cached UUID belongs to a different manifest source"});
            const auto previous = directories.find({info->mount, info->path.Parent().GetStr()});
            if (previous != directories.end())
                block(previous->second, "Cached identity conflicts with an authoritative source");
            std::lock_guard publish(assetMutex);
            pathMap[info->mount].erase(info->path);
            cached->second->revision->fetch_add(1, std::memory_order_acq_rel);
            InvalidatePath(info->path);
            idMap.erase(cached);
            generatedManifests.erase(id);
            std::erase_if(generatedAssets, [&](const auto &child) { return child.second.owner == id; });
        }
        for (auto &[key, directory] : directories) {
            const auto &fs = directory.filesystem;
            FileLockPtr fileLock;
            const bool  missing = std::any_of(directory.sources.begin(), directory.sources.end(),
                                              [&](const auto &path) { return !directory.manifest.Find(path.FileName()); });
            if (missing && directory.writable && !directory.blocked) {
                const auto status = AcquireManifestLock(fs, directory.path, fileLock, cancelled);
                if (!status) {
                    block(directory, status.message, true);
                    report.errors.push_back({fs->GetPath() / directory.path, std::make_error_code(std::errc::resource_unavailable_try_again)});
                    continue;
                }
                if (const auto read = manifests.Reload(fs, directory.path, directory.manifest); !read) {
                    block(directory, read.message);
                    report.warnings.push_back({directory.mount, directory.path / FilePath("assets.jsonl"), 0, read.message});
                    directory.manifest = AssetIndexFile::Load(fs, directory.path / FilePath("assets.jsonl"), "file", "cook");
                }
                manifests.Invalidate(fs, directory.path);
                for (const auto &entry : directory.manifest.Entries()) {
                    const auto owner = owners.find(entry.id);
                    if (owner != owners.end() && owner->second != &directory) {
                        conflicts.insert(entry.id);
                        block(directory, "Source UUID changed to another directory's identity");
                        block(*owner->second, "Source UUID is duplicated across directories");
                    }
                    GeneratedAssetManifest generated;
                    if (const auto parsed = ParseGeneratedAssets(entry.id, entry.generated, generated); !parsed)
                        block(directory, parsed.message);
                }
            }
            std::vector<IndexFileEntry> additions;
            for (const auto &path : directory.sources) {
                if (directory.manifest.Find(path.FileName()) || directory.blocked || !directory.writable)
                    continue;
                const auto existing = FindSource(directory.mount, path);
                const auto id       = existing ? existing->uuid : migrationMode ? CalculateLegacyUuid(directory.mount, path) : Uuid::Create();
                if (conflicts.contains(id) || generatedAssets.contains(id) || owners.contains(id)) {
                    block(directory, "Cached source UUID conflicts with a manifest identity");
                    break;
                }
                additions.push_back({path.FileName(), id, {}});
            }
            if (!directory.blocked && !additions.empty()) {
                directory.manifest.SetEntries(additions);
                if (const auto saved = manifests.Save(fs, directory.path, directory.manifest); !saved) {
                    block(directory, saved.status.message, true);
                    for (const auto &issue : saved.recovery)
                        report.warnings.push_back({directory.mount, issue.path, 0, issue.stage + ": " + issue.status.message});
                    report.errors.push_back({fs->GetPath() / directory.path, std::make_error_code(std::errc::io_error)});
                    continue;
                }
            }
            fileLock.reset();
            std::lock_guard publish(assetMutex);
            for (const auto &path : directory.sources) {
                const auto *entry = directory.manifest.Find(path.FileName());
                if ((!entry && directory.blocked) || (entry && conflicts.contains(entry->id))) {
                    const auto previous = FindSource(directory.mount, path);
                    if (previous) {
                        generatedManifests.erase(previous->uuid);
                        std::erase_if(generatedAssets, [&](const auto &child) { return child.second.owner == previous->uuid; });
                        previous->revision->fetch_add(1, std::memory_order_acq_rel);
                        InvalidatePath(path);
                        idMap.erase(previous->uuid);
                        pathMap[directory.mount].erase(path);
                    }
                    continue;
                }
                auto       info = FindSource(directory.mount, path);
                const auto id   = entry ? entry->id : info ? info->uuid : CalculateUuidByPath(path);
                if (info && info->uuid != id) {
                    report.warnings.push_back({directory.mount, path, 0, "Manifest identity supersedes cached source identity"});
                    generatedManifests.erase(info->uuid);
                    std::erase_if(generatedAssets, [&](const auto &child) { return child.second.owner == info->uuid; });
                    info->revision->fetch_add(1, std::memory_order_acq_rel);
                    InvalidatePath(path);
                    idMap.erase(info->uuid);
                    pathMap[directory.mount].erase(path);
                    info = {};
                }
                const auto existing = idMap.find(id);
                if ((existing != idMap.end() && (existing->second->mount != directory.mount || existing->second->path != path)) ||
                    generatedAssets.contains(id)) {
                    block(directory, "Source identity conflicts with the published catalog");
                    continue;
                }
                if (!info) {
                    info        = new AssetSourceInfo;
                    info->uuid  = id;
                    info->path  = path;
                    info->mount = directory.mount;
                    info->ext   = path.Extension();
                    InvalidatePath(path);
                    idMap.emplace(id, info);
                    pathMap[directory.mount].emplace(path, id);
                }
                if ((entry && !entry->generated.empty()) || generatedManifests.contains(id)) {
                    GeneratedAssetManifest generated;
                    const auto             parsed = ParseGeneratedAssets(id, entry ? entry->generated : std::string{}, generated);
                    if (!parsed || !PublishGeneratedCatalog(id, generated)) {
                        generatedManifests.erase(id);
                        std::erase_if(generatedAssets, [&](const auto &child) { return child.second.owner == id; });
                        block(directory, "Generated catalog requires repair");
                    }
                }
            }
        }
        std::lock_guard publish(assetMutex);
        for (const auto &id : conflicts) {
            const auto found = idMap.find(id);
            if (found != idMap.end()) {
                pathMap[found->second->mount].erase(found->second->path);
                found->second->revision->fetch_add(1, std::memory_order_acq_rel);
                InvalidatePath(found->second->path);
                idMap.erase(found);
            }
            generatedManifests.erase(id);
            std::erase_if(generatedAssets, [&](const auto &child) { return child.second.owner == id; });
        }
        for (const auto &warning : report.warnings)
            LOG_W(TAG, "%s:%s:%zu: %s", warning.mount.c_str(), warning.path.GetStr().c_str(), warning.line, warning.message.c_str());
        for (const auto &directory : report.blockedDirs)
            LOG_W(TAG, "Blocked %s:%s: %s", directory.mount.c_str(), directory.directory.GetStr().c_str(), directory.reason.c_str());
        return report;
    }

    AssetDataBase::SourceScanResult AssetDataBase::ScanSources() const
    {
        auto extensions = AssetBuilderManager::Get()->GetExtensions();
        extensions.push_back(".world");

        // Complete every mount before publishing assets or seeding manifests.
        std::vector<MountedPath> files;
        for (size_t index = 0; index < mountFsList.size(); ++index) {
            const auto &fs = mountFsList[index];
            if (fs == nullptr) {
                continue;
            }
            auto scanned = NativeFileSystem::FilterFiles(fs->GetPath(), extensions);
            if (const auto *error = std::get_if<FileScanError>(&scanned)) {
                return *error;
            }
            auto &paths = std::get<std::vector<FilePath>>(scanned);
            for (auto &path : paths) {
                files.emplace_back(mountInfos[index].id, std::move(path));
            }
        }
        return files;
    }

    void AssetDataBase::BuildAllTargets(const Uuid &uuid)
    {
        const auto snapshot = CaptureSource(uuid);
        if (!snapshot.source)
            return;
        const auto cookCfg = AssetBuilderManager::Get()->GetCookConfig();
        const auto targets = cookCfg.GetTargets(CookJsonFor(snapshot), cookCfg.GetActivePlatform());
        for (const auto &target : targets)
            AssetBuilderManager::Get()->RequestCook(uuid, target);
    }

    std::string AssetDataBase::CookJsonFor(const AssetSourceSnapshot &snapshot) const
    {
        if (!snapshot.source || !snapshot.filesystem)
            return {};
        const auto  manifest = manifests.Get(snapshot.filesystem, snapshot.source->path.Parent());
        const auto *entry    = manifest->Find(snapshot.source->path.FileName());
        return entry && entry->id == snapshot.source->uuid ? entry->extra : std::string{};
    }

    std::string AssetDataBase::GetCookJson(const Uuid &id) const
    {
        Uuid owner;
        return GetOwner(id, owner) ? CookJsonFor(CaptureSource(owner)) : std::string{};
    }

    AssetMutationResult AssetDataBase::SetCookJson(const Uuid &id, const std::string &cookJson)
    {
        std::unique_lock writeLock(*mutationMutex);
        const auto       cancelled = SourceCancellation();
        const auto       source    = FindSource(id);
        FileSystemPtr    filesystem;
        if (const auto status = WritableSource(source, filesystem); !status)
            return {status, "validate"};
        SourceManifestTransaction transaction(manifests, filesystem, source->path.Parent(), cancelled);
        if (const auto status = transaction.Open(); !status)
            return {status, "open-manifest"};
        auto       &manifest = transaction.manifest;
        const auto *entry    = manifest.Find(source->path.FileName());
        if (!entry || entry->id != id)
            return {FileOperationError::AlreadyExists, "Cook settings require the unchanged manifest identity"};
        auto updated  = *entry;
        updated.extra = cookJson;
        manifest.Set(updated);
        auto result = CommitResult(transaction.Commit(manifest), source);
        if (result)
            source->revision->fetch_add(1, std::memory_order_acq_rel);
        return result;
    }

    bool AssetDataBase::GetAbsoluteSourcePath(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        const auto snapshot = CaptureSource(owner);
        if (!snapshot.source || !snapshot.filesystem)
            return false;
        out = (snapshot.filesystem->GetPath() / snapshot.source->path).GetStr();
        return true;
    }

    void AssetDataBase::Dump(std::ostream &stream)
    {
        const auto snapshot = GetSources();
        for (const auto &[id, info] : snapshot) {
            stream << id.ToString() << "\t" << QueryType(info->ext) << "\t" << info->name << "\t" << info->path.GetStr() << "\n";
        }
    }

} // namespace sky
