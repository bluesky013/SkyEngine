//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetDataBase.h>

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

static const char *TAG = "AssetDataBase";

namespace sky {

    void AssetDataBase::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        engineFs = fs ? CastPtr<NativeFileSystem>(fs->CreateSubSystem("assets", true)) : NativeFileSystemPtr{};
        RebuildMounts();
    }

    void AssetDataBase::SetWorkSpaceFs(const FileSystemPtr &fs)
    {
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

    std::optional<FileScanError> AssetDataBase::MigrateLegacyIdentity()
    {
        // Seed legacy `(bundle, path)` UUIDs into every source manifest. Existing entries and any
        // `assets.db` rows win (idempotent); only directories containing assets get a manifest.
        migrationMode    = true;
        const auto error = RebuildCacheFromScan();
        migrationMode    = false;
        return error;
    }

    AssetSourcePtr AssetDataBase::FindAsset(const Uuid &id)
    {
        std::lock_guard lock(assetMutex);
        auto            iter = idMap.find(id);
        return iter != idMap.end() ? iter->second : nullptr;
    }

    AssetSourcePtr AssetDataBase::FindAsset(const FilePath &path)
    {
        std::lock_guard lock(assetMutex);
        for (const auto &mount : mountInfos) {
            if (const auto source = FindAsset(mount.id, path)) {
                return source;
            }
        }
        return {};
    }

    AssetSourcePtr AssetDataBase::FindAsset(const std::string &mount, const FilePath &path)
    {
        std::lock_guard lock(assetMutex);
        const auto      bucket = pathMap.find(mount);
        if (bucket == pathMap.end()) {
            return {};
        }
        const auto found = bucket->second.find(path);
        return found != bucket->second.end() ? idMap.at(found->second) : AssetSourcePtr{};
    }

    std::vector<AssetSourcePtr> AssetDataBase::Gather(const std::string_view &category)
    {
        std::vector<AssetSourcePtr> res;

        std::lock_guard lock(assetMutex);
        for (auto &[id, asset] : idMap) {
            if (QueryType(asset->ext) == category) {
                res.emplace_back(asset);
            }
        }

        return res;
    }

    void AssetDataBase::ForEachSource(const std::function<void(const AssetSourcePtr &)> &fn) const
    {
        if (!fn) {
            return;
        }

        std::vector<AssetSourcePtr> snapshot;
        {
            std::lock_guard lock(assetMutex);
            snapshot.reserve(idMap.size());
            for (const auto &[id, asset] : idMap) {
                snapshot.emplace_back(asset);
            }
        }

        for (const auto &asset : snapshot) {
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
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(id);
        if (src == nullptr) {
            return false;
        }
        out = QueryType(src->ext);
        return !out.empty();
    }

    bool AssetDataBase::GetTarget(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(owner);
        out      = AssetBuilderManager::Get()->GetCookConfig().ResolveTarget(CookJsonFor(src));
        return true;
    }

    bool AssetDataBase::GetSourcePath(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(owner);
        if (src == nullptr) {
            return false;
        }
        out = src->path.GetStr();
        return true;
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const FilePath &path, bool build)
    {
        std::lock_guard lock(assetMutex);
        const int       index = FindMountIndex(path);
        return index >= 0 ? RegisterAsset(mountInfos[index].id, path, build) : AssetSourcePtr{};
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const std::string &mount, const FilePath &path, bool build)
    {
        std::lock_guard lock(assetMutex);
        const int       index = FindMountIndex(mount);
        if (index < 0 || !mountFsList[index] || !mountFsList[index]->FileExist(path)) {
            LOG_E(TAG, "Source is missing from mount %s: %s", mount.c_str(), path.GetStr().c_str());
            return {};
        }
        const auto  fs        = mountFsList[index];
        const bool  writable  = mountInfos[index].writable || migrationMode;
        const auto  directory = path.Parent();
        FileLockPtr fileLock;
        if (writable && !fs->AcquireFileLock(directory / FilePath("assets.jsonl.lock"), fileLock)) {
            LOG_E(TAG, "Cannot lock source manifest: %s", path.GetStr().c_str());
            return {};
        }
        AssetIndexFile manifest("file", "cook");
        if (const auto status = manifests.Reload(fs, directory, manifest); !status) {
            LOG_E(TAG, "%s: %s", path.GetStr().c_str(), status.message.c_str());
            return {};
        }
        const auto *entry = manifest.Find(path.FileName());
        auto        info  = FindAsset(mount, path);
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
            if (!manifests.Save(fs, directory, manifest)) {
                LOG_E(TAG, "Cannot persist source identity: %s", path.GetStr().c_str());
                return {};
            }
        }
        if (!info) {
            info        = new AssetSourceInfo();
            info->path  = path;
            info->uuid  = uuid;
            info->ext   = path.Extension();
            info->mount = mount;
            idMap.emplace(uuid, info);
            pathMap[mount].emplace(path, uuid);
        }
        fileLock.reset();
        if (!RefreshOwner(info->uuid)) {
            LOG_E(TAG, "Generated catalog refresh failed for %s", info->uuid.ToString().c_str());
        }
        if (build) {
            AssetBuildRequest request = {};
            request.file              = fs->OpenFile(path);
            request.assetInfo         = info;
            AssetBuilderManager::Get()->BuildRequest(request);
        }
        return info;
    }

    FileOperationStatus AssetDataBase::PersistSourceIdentity(const Uuid &id)
    {
        std::lock_guard lock(assetMutex);
        const auto      found = idMap.find(id);
        if (found == idMap.end() || !found->second) {
            return {FileOperationError::InvalidPath, "Source identity is not registered"};
        }
        const auto &source = found->second;
        const int   index  = FindMountIndex(source->mount);
        if (index < 0 || index >= static_cast<int>(mountInfos.size()) || !mountInfos[index].writable || !mountFsList[index] ||
            mountInfos[index].id != source->mount) {
            return {FileOperationError::InvalidPath, "Source identity requires its writable owning mount"};
        }
        const auto &filesystem = mountFsList[index];
        const auto  directory  = source->path.Parent();
        FileLockPtr fileLock;
        if (const auto status = filesystem->AcquireFileLock(directory / FilePath("assets.jsonl.lock"), fileLock); !status) {
            return status;
        }
        AssetIndexFile manifest("file", "cook");
        if (const auto status = manifests.Reload(filesystem, directory, manifest); !status)
            return status;
        const auto *entry = manifest.Find(source->path.FileName());
        if (entry && entry->id != id) {
            return {FileOperationError::AlreadyExists, "Source manifest belongs to a different identity"};
        }
        if (!entry) {
            IndexFileEntry added;
            added.key = source->path.FileName();
            added.id  = id;
            manifest.Set(added);
        }
        if (!manifests.Save(filesystem, directory, manifest)) {
            return {FileOperationError::IOError, "Source identity could not be persisted"};
        }
        return {};
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

    FileOperationStatus AssetDataBase::RemoveAsset(const Uuid &id)
    {
        return RemoveSource(id, false);
    }

    FileOperationStatus AssetDataBase::DeleteAsset(const Uuid &id)
    {
        return RemoveSource(id, true);
    }

    FileOperationStatus AssetDataBase::RemoveSource(const Uuid &id, bool deleteFile)
    {
        std::lock_guard lock(assetMutex);
        const auto      source = FindAsset(id);
        FileSystemPtr   fs;
        if (const auto status = WritableSource(source, fs); !status) {
            return status;
        }
        const auto  path      = source->path;
        const auto  directory = path.Parent();
        FileLockPtr fileLock;
        if (const auto status = fs->AcquireFileLock(directory / FilePath("assets.jsonl.lock"), fileLock); !status) {
            return status;
        }
        AssetIndexFile original("file", "cook");
        if (const auto status = manifests.Reload(fs, directory, original); !status)
            return status;
        const auto *entry = original.Find(path.FileName());
        if (!entry || entry->id != id) {
            return {FileOperationError::AlreadyExists, "Source manifest identity is missing or belongs to another asset"};
        }
        const auto unpublish = [&] {
            pathMap[source->mount].erase(path);
            idMap.erase(id);
            for (auto &[childId, record] : generatedAssets) {
                if (record.owner == id) {
                    record.active = false;
                }
            }
            generatedManifests.erase(id);
        };
        const FilePath temporary(path.GetStr() + ".deleted-" + Uuid::Create().ToString());
        if (deleteFile && !fs->Rename(path, temporary)) {
            return {FileOperationError::IOError, "Could not stage source deletion"};
        }
        auto       manifest = original;
        const bool changed  = manifest.Remove(path.FileName());
        if (changed && !manifests.Save(fs, directory, manifest)) {
            const bool restored = !deleteFile || fs->Rename(temporary, path);
            if (!restored)
                unpublish();
            return {FileOperationError::IOError, restored
                                                     ? "Could not remove source identity from its manifest"
                                                     : "Manifest removal failed; recover staged source at " + (fs->GetPath() / temporary).GetStr()};
        }
        if (deleteFile) {
            if (const auto status = fs->RemoveFile(temporary); !status) {
                const bool restoredManifest = !changed || manifests.Save(fs, directory, original);
                const bool restoredFile     = restoredManifest && fs->Rename(temporary, path);
                if (!restoredFile)
                    unpublish();
                return {status.error, status.message + (restoredFile ? "; deletion rolled back"
                                                                     : "; deletion requires recovery from " + (fs->GetPath() / temporary).GetStr())};
            }
        }
        unpublish();
        return {};
    }

    AssetSourcePtr AssetDataBase::MoveAsset(const FilePath &from, const FilePath &to)
    {
        std::lock_guard lock(assetMutex);
        const auto      source = FindAsset(from);
        FileSystemPtr   fs;
        if (const auto status = WritableSource(source, fs); !status) {
            LOG_E(TAG, "%s", status.message.c_str());
            return {};
        }
        const std::filesystem::path target(to.GetStr());
        if (target.empty() || target.is_absolute() || target.has_root_name() || target.lexically_normal() != target ||
            to.GetStr().find('\\') != std::string::npos || to.GetStr().find(':') != std::string::npos ||
            to.GetStr().find('\0') != std::string::npos || std::any_of(target.begin(), target.end(), [](const auto &part) { return part == ".."; }) ||
            FindAsset(source->mount, to) || fs->FileExist(to)) {
            LOG_E(TAG, "MoveAsset destination is invalid or occupied: %s", to.GetStr().c_str());
            return {};
        }
        std::vector<FilePath> directories{from.Parent()};
        if (to.Parent() != from.Parent()) {
            directories.push_back(to.Parent());
        }
        std::sort(directories.begin(), directories.end(), [](const auto &a, const auto &b) { return a.GetStr() < b.GetStr(); });
        std::vector<FileLockPtr> fileLocks;
        for (const auto &directory : directories) {
            FileLockPtr fileLock;
            if (const auto status = fs->AcquireFileLock(directory / FilePath("assets.jsonl.lock"), fileLock); !status) {
                LOG_E(TAG, "MoveAsset manifest lock failed: %s", status.message.c_str());
                return {};
            }
            fileLocks.push_back(std::move(fileLock));
            manifests.Invalidate(fs, directory);
        }
        AssetIndexFile original("file", "cook"), destination("file", "cook");
        if (!manifests.Reload(fs, from.Parent(), original) || !manifests.Reload(fs, to.Parent(), destination)) {
            LOG_E(TAG, "MoveAsset could not read complete source manifests");
            return {};
        }
        if (!fs->FileExist(from) || fs->FileExist(to)) {
            LOG_E(TAG, "MoveAsset source or destination changed before locking");
            return {};
        }
        const auto *entry = original.Find(from.FileName());
        if (!entry || entry->id != source->uuid || destination.Find(to.FileName())) {
            LOG_E(TAG, "MoveAsset manifest identity conflict");
            return {};
        }
        auto moved   = entry ? *entry : IndexFileEntry{};
        moved.key    = to.FileName();
        moved.id     = source->uuid;
        auto updated = original;
        updated.Remove(from.FileName());
        auto next = from.Parent() == to.Parent() ? updated : destination;
        next.Set(moved);
        if (!fs->Rename(from, to)) {
            LOG_E(TAG, "MoveAsset rename failed %s -> %s", from.GetStr().c_str(), to.GetStr().c_str());
            return {};
        }
        const bool sameDirectory    = from.Parent() == to.Parent();
        const bool destinationSaved = manifests.Save(fs, to.Parent(), next);
        if (!destinationSaved || (!sameDirectory && !manifests.Save(fs, from.Parent(), updated))) {
            const bool restoredDestination = !destinationSaved || manifests.Save(fs, to.Parent(), destination);
            const bool restoredFile        = fs->Rename(to, from);
            LOG_E(TAG, "MoveAsset manifest write failed; rollback %s",
                  restoredDestination && restoredFile ? "completed" : "failed; source files require recovery");
            return {};
        }
        pathMap[source->mount].erase(from);
        source->path = to;
        pathMap[source->mount].emplace(to, source->uuid);
        return source;
    }

    AssetSourcePtr AssetDataBase::DuplicateAsset(const FilePath &from, const FilePath &to)
    {
        const auto original = FindAsset(from);
        const auto fs       = ResolveOwningFs(original);
        if (fs == nullptr || !fs->FileExist(from)) {
            LOG_E(TAG, "DuplicateAsset source missing %s", from.GetStr().c_str());
            return nullptr;
        }

        const auto           input = fs->OpenFile(from);
        std::vector<uint8_t> bytes;
        if (!input || !input->ReadBin(bytes) || !workSpaceFs || !workSpaceFs->WriteFileExclusive(to, bytes)) {
            LOG_E(TAG, "DuplicateAsset failed to create destination %s", to.GetStr().c_str());
            return {};
        }
        const auto copy = RegisterAsset("workspace", to, false);
        if (!copy || !original) {
            return copy;
        }
        const auto *entry    = manifests.Get(fs, from.Parent()).Find(from.FileName());
        const auto  metadata = entry ? *entry : IndexFileEntry{};
        if (!metadata.extra.empty() && !SetCookJson(copy->uuid, metadata.extra)) {
            return {};
        }
        GeneratedAssetManifest generated;
        if (!ParseGeneratedAssets(original->uuid, metadata.generated, generated)) {
            return {};
        }
        std::vector<GeneratedAssetDescriptor> requested;
        for (const auto &record : generated.records) {
            if (record.active) {
                requested.push_back({record.key, record.type});
            }
        }
        GeneratedAssetManifest duplicated;
        if (!requested.empty() && !UpdateGeneratedProducts(copy->uuid, requested, generated.defaults, duplicated)) {
            return {};
        }
        return copy;
    }

    AssetSourcePtr AssetDataBase::ImportAsset(const FilePath &sourceFile, const FilePath &dest, bool cook)
    {
        auto in = sourceFile.OpenIFStream(std::ios::binary);
        if (!in) {
            LOG_E(TAG, "Import source missing %s", sourceFile.GetStr().c_str());
            return nullptr;
        }

        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // Close the destination handle before registering/cooking.
        {
            auto out = CreateOrOpenFile(dest);
            if (out == nullptr) {
                LOG_E(TAG, "Import failed to create %s", dest.GetStr().c_str());
                return nullptr;
            }
            out->AppendData(data.data(), data.size());
        }

        return RegisterAsset(dest, cook);
    }

    FilePtr AssetDataBase::OpenFile(const AssetSourcePtr &src)
    {
        const auto fs = ResolveOwningFs(src);
        return fs ? fs->OpenFile(src->path) : FilePtr{};
    }

    FilePtr AssetDataBase::OpenFile(const FilePath &path)
    {
        return mountFs->OpenFile(path);
    }

    FilePtr AssetDataBase::CreateOrOpenFile(const FilePath &path)
    {
        return mountFs->CreateOrOpenFile(path);
    }

    void AssetDataBase::SetBuildDependencies(const Uuid &id, const std::vector<Uuid> &dependencies)
    {
        std::lock_guard lock(assetMutex);
        const auto      entry = idMap.find(id);
        if (entry != idMap.end()) {
            entry->second->dependencies = dependencies;
        }
    }

    void AssetDataBase::SetMarkedName(const Uuid &id, const std::string &name)
    {
        auto iter = idMap.find(id);
        if (iter == idMap.end()) {
            return;
        }

        iter->second->name = name;
    }

    std::optional<FileScanError> AssetDataBase::Load()
    {
        if (workSpaceFs == nullptr) {
            return {};
        }

        const auto scanned = ScanSources();
        if (const auto *error = std::get_if<FileScanError>(&scanned)) {
            return *error;
        }
        auto file = workSpaceFs->OpenFile("assets.db");
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

                    std::lock_guard lock(assetMutex);
                    if (info.mount.empty()) {
                        for (size_t index = 0; index < mountFsList.size(); ++index) {
                            const auto &fs = mountFsList[index];
                            manifests.Invalidate(fs, info.path.Parent());
                            const auto *entry = manifests.Get(fs, info.path.Parent()).Find(info.path.FileName());
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
                    const auto *entry = manifests.Get(fs, info.path.Parent()).Find(info.path.FileName());
                    if ((entry && entry->id != info.uuid) || idMap.contains(info.uuid) || generatedAssets.contains(info.uuid)) {
                        continue;
                    }
                    pathMap[info.mount].emplace(info.path, info.uuid);
                    idMap.emplace(info.uuid, pInfo);
                }

                json.End();
            }
        }

        for (const auto &[mount, relative] : std::get<std::vector<MountedPath>>(scanned)) {
            if (!RegisterAsset(mount, relative, false)) {
                const int index = FindMountIndex(mount);
                return FileScanError{mountFsList[index]->GetPath() / relative, std::make_error_code(std::errc::io_error)};
            }
        }
        return {};
    }

    void AssetDataBase::Save()
    {
        AssetExecutor::Get()->WaitForAll();

        if (workSpaceFs == nullptr) {
            return;
        }

        auto              file    = workSpaceFs->CreateOrOpenFile("assets.db");
        auto              archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);

        {
            std::lock_guard lock(assetMutex);

            json.StartObject();
            json.Key("assets");
            json.StartArray();
            for (auto &[id, pInfo] : idMap) {
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
        std::lock_guard lock(assetMutex);
        pathMap.clear();
        idMap.clear();
        generatedAssets.clear();
        generatedManifests.clear();
        manifests.Clear();
    }

    std::optional<FileScanError> AssetDataBase::RebuildCacheFromScan()
    {
        const auto scanned = ScanSources();
        if (const auto *error = std::get_if<FileScanError>(&scanned)) {
            return *error;
        }
        for (const auto &[mount, relative] : std::get<std::vector<MountedPath>>(scanned)) {
            if (!RegisterAsset(mount, relative, false)) {
                const int index = FindMountIndex(mount);
                return FileScanError{mountFsList[index]->GetPath() / relative, std::make_error_code(std::errc::io_error)};
            }
        }
        return {};
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
        auto src = FindAsset(uuid);
        if (src == nullptr) {
            return;
        }

        const auto cookCfg = AssetBuilderManager::Get()->GetCookConfig();
        const auto targets = cookCfg.GetTargets(CookJsonFor(src), cookCfg.GetActivePlatform());
        for (const auto &target : targets) {
            AssetBuilderManager::Get()->RequestCook(uuid, target);
        }
    }

    std::string AssetDataBase::CookJsonFor(const AssetSourcePtr &src) const
    {
        if (src == nullptr) {
            return {};
        }

        const auto fs = ResolveOwningFs(src);
        if (fs == nullptr) {
            return {};
        }

        const auto *entry = manifests.Get(fs, src->path.Parent()).Find(src->path.FileName());
        return entry != nullptr ? entry->extra : std::string{};
    }

    std::string AssetDataBase::GetCookJson(const Uuid &id) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return {};
        }
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(owner);
        return CookJsonFor(src);
    }

    bool AssetDataBase::SetCookJson(const Uuid &id, const std::string &cookJson)
    {
        std::lock_guard lock(assetMutex);

        auto iter = idMap.find(id);
        if (iter == idMap.end() || iter->second == nullptr) {
            return false;
        }
        const AssetSourcePtr &src = iter->second;

        const int index = FindMountIndex(src->mount);
        if (index < 0 || index >= static_cast<int>(mountInfos.size()) || !mountInfos[index].writable) {
            return false; // read-only mount or unmounted path
        }
        const FileSystemPtr &fs = mountFsList[index];
        if (fs == nullptr) {
            return false;
        }

        const auto dir      = src->path.Parent();
        const auto fileName = src->path.FileName();

        FileLockPtr fileLock;
        if (!fs->AcquireFileLock(dir / FilePath("assets.jsonl.lock"), fileLock)) {
            return false;
        }
        AssetIndexFile manifest("file", "cook");
        if (!manifests.Reload(fs, dir, manifest))
            return false;
        const auto *entry = manifest.Find(fileName);
        if (entry == nullptr || entry->id != id) {
            return false; // no matching manifest identity to attach the cook block to
        }

        IndexFileEntry updated = *entry;
        updated.extra          = cookJson;
        manifest.Set(updated);
        return manifests.Save(fs, dir, manifest);
    }

    bool AssetDataBase::GetAbsoluteSourcePath(const Uuid &id, std::string &out) const
    {
        Uuid owner;
        if (!GetOwner(id, owner)) {
            return false;
        }
        auto *self = const_cast<AssetDataBase *>(this);
        auto  src  = self->FindAsset(owner);
        if (!src) {
            return false;
        }
        const int index = FindMountIndex(src->mount);
        if (index < 0 || mountFsList[index] == nullptr) {
            return false;
        }
        out = mountFsList[index]->GetPath().GetStr() + "/" + src->path.GetStr();
        return true;
    }

    void AssetDataBase::Dump(std::ostream &stream)
    {
        std::lock_guard lock(assetMutex);
        for (auto &[id, info] : idMap) {
            stream << id.ToString() << "\t" << QueryType(info->ext) << "\t" << info->name << "\t" << info->path.GetStr() << "\n";
        }
    }

} // namespace sky
