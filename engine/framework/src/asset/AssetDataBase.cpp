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

    FileSystemPtr AssetDataBase::ResolveOwningFs(const FilePath &path) const
    {
        for (const auto &fs : mountFsList) {
            if (fs != nullptr && fs->FileExist(path)) {
                return fs;
            }
        }
        return {};
    }

    Uuid AssetDataBase::CalculateUuidByPath(const FilePath &path)
    {
        return Uuid::CreateWithSeed(Fnv1a32(path.GetStr()));
    }

    Uuid AssetDataBase::CalculateLegacyUuid(const FilePath &path) const
    {
        // Reproduce the legacy identity: Uuid::CreateWithSeed(HashCombine32(bundle, Fnv1a32(path))).
        // Legacy SourceAssetBundle ordinals: INVALID=0, ENGINE=1, WORKSPACE=2; map the mount role.
        const int      index  = FindMountIndex(path);
        const uint32_t bundle = (index >= 0 && mountInfos[index].id == "engine") ? 1u : 2u; // ENGINE : WORKSPACE
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
        {
            std::lock_guard lock(assetMutex);
            auto            iter = pathMap.find(path);
            if (iter != pathMap.end()) {
                return idMap.at(iter->second);
            }
        }

        auto fs = ResolveOwningFs(path);
        if (fs != nullptr) {
            const auto *entry = manifests.Get(fs, path.Parent()).Find(path.FileName());
            if (entry != nullptr) {
                return FindAsset(entry->id);
            }
        }
        return {};
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

    void AssetDataBase::EnsureManifestEntry(const FileSystemPtr &fs, const FilePath &path, const Uuid &uuid)
    {
        const auto dir      = path.Parent();
        const auto fileName = path.FileName();

        auto manifest = manifests.Get(fs, dir);
        if (manifest.Find(fileName) != nullptr) {
            return;
        }

        IndexFileEntry entry;
        entry.key = fileName;
        entry.id  = uuid;
        manifest.Set(entry);
        manifests.Save(fs, dir, manifest);
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
        auto fs = ResolveOwningFs(path);
        if (fs == nullptr) {
            LOG_E(TAG, "File not Exist %s", path.GetStr().c_str());
            return nullptr;
        }

        AssetSourcePtr info = nullptr;
        {
            std::lock_guard lock(assetMutex);
            auto            iter = pathMap.find(path);
            if (iter != pathMap.end()) {
                info = idMap.at(iter->second);
            }
        }

        const int         mountIndex = FindMountIndex(path);
        const std::string mountId    = mountIndex >= 0 ? mountInfos[mountIndex].id : std::string{};
        // "Read-only" is a property of the owning mount (the engine bundle), not of the underlying
        // filesystem object; read-only mounts never get manifests written.
        const bool readOnly = mountIndex < 0 || !mountInfos[mountIndex].writable;

        if (info != nullptr) {
            // Legacy asset (assets.db row): keep its identity and seed the manifest on first write
            // (writable mounts only).
            if (info->mount.empty()) {
                info->mount = mountId;
            }
            if (!readOnly) {
                EnsureManifestEntry(fs, path, info->uuid);
            }
        } else {
            const auto  dir      = path.Parent();
            const auto  fileName = path.FileName();
            const auto *entry    = manifests.Get(fs, dir).Find(fileName);

            // Prefer the manifest UUID; during migration seed the legacy (bundle, path) UUID; otherwise
            // a read-only mount without a manifest keeps stable path-derived identity, and a writable
            // mount assigns a fresh UUID on first registration.
            const Uuid uuid =
                entry != nullptr ? entry->id : (migrationMode ? CalculateLegacyUuid(path) : (readOnly ? CalculateUuidByPath(path) : Uuid::Create()));
            auto ext = path.Extension();

            AssetSourcePtr srcInfo = new AssetSourceInfo();
            srcInfo->path          = path;
            srcInfo->uuid          = uuid;
            srcInfo->ext           = ext;
            srcInfo->mount         = mountId;

            {
                std::lock_guard lock(assetMutex);
                info = idMap.emplace(uuid, std::move(srcInfo)).first->second;
                pathMap.emplace(path, uuid);
            }

            // Write a manifest entry for writable mounts, and for any mount during the migration (the
            // committed engine tree manifests are produced by migration).
            if (entry == nullptr && (migrationMode || !readOnly)) {
                EnsureManifestEntry(fs, path, uuid);
            }
        }

        if (!RefreshOwner(info->uuid)) {
            LOG_E(TAG, "Generated catalog refresh failed for %s", info->uuid.ToString().c_str());
        }
        if (build) {
            AssetBuildRequest request = {};
            request.file              = fs->OpenFile(path);
            request.assetInfo         = info;
            LOG_I(TAG, "Request Build Asset %s", info->uuid.ToString().c_str());
            AssetBuilderManager::Get()->BuildRequest(request);
        }

        return info;
    }

    void AssetDataBase::RemoveAsset(const Uuid &id)
    {
        FilePath path;
        {
            std::lock_guard lock(assetMutex);
            auto            iter = idMap.find(id);
            if (iter == idMap.end()) {
                return;
            }
            path = iter->second->path;
            pathMap.erase(path);
            idMap.erase(iter);
            for (auto &[childId, record] : generatedAssets) {
                if (record.owner == id) {
                    record.active = false;
                }
            }
            generatedManifests.erase(id);
        }

        auto fs = ResolveOwningFs(path);
        if (fs == nullptr) {
            fs = workSpaceFs; // file may already be gone; mutations target the writable mount
        }
        if (fs != nullptr) {
            auto manifest = manifests.Get(fs, path.Parent());
            if (manifest.Remove(path.FileName())) {
                manifests.Save(fs, path.Parent(), manifest);
            }
        }
    }

    AssetSourcePtr AssetDataBase::MoveAsset(const FilePath &from, const FilePath &to)
    {
        auto fs = ResolveOwningFs(from);
        if (fs == nullptr || !fs->FileExist(from)) {
            LOG_E(TAG, "MoveAsset source missing %s", from.GetStr().c_str());
            return nullptr;
        }

        if (!fs->Rename(from, to)) {
            LOG_E(TAG, "MoveAsset rename failed %s -> %s", from.GetStr().c_str(), to.GetStr().c_str());
            return nullptr;
        }

        Uuid           uuid;
        IndexFileEntry moved;
        {
            const auto &manifest = manifests.Get(fs, from.Parent());
            const auto *entry    = manifest.Find(from.FileName());
            uuid                 = entry != nullptr ? entry->id : Uuid::Create();
            moved                = entry != nullptr ? *entry : IndexFileEntry{};

            auto updated = manifest;
            updated.Remove(from.FileName());
            manifests.Save(fs, from.Parent(), updated);
        }
        {
            auto dest = manifests.Get(fs, to.Parent());
            moved.key = to.FileName();
            moved.id  = uuid;
            dest.Set(moved);
            manifests.Save(fs, to.Parent(), dest);
        }

        AssetSourcePtr info;
        {
            std::lock_guard lock(assetMutex);
            auto            iter = pathMap.find(from);
            if (iter != pathMap.end()) {
                info = idMap.at(iter->second);
                pathMap.erase(iter);
                info->path = to;
                pathMap.emplace(to, uuid);
            }
        }
        return info;
    }

    AssetSourcePtr AssetDataBase::DuplicateAsset(const FilePath &from, const FilePath &to)
    {
        auto fs = ResolveOwningFs(from);
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
        const auto copy     = RegisterAsset(to, false);
        const auto original = FindAsset(from);
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
        return mountFs->OpenFile(src->path);
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
                    auto *pInfo = new AssetSourceInfo();
                    auto &info  = *pInfo;

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

                    uint32_t depCount = json.StartArray("dependencies");
                    for (uint32_t j = 0; j < depCount; ++j) {
                        info.dependencies.emplace_back(Uuid::CreateFromString(json.LoadString()));
                        json.NextArrayElement();
                    }
                    json.End();

                    json.NextArrayElement();

                    {
                        std::lock_guard lock(assetMutex);
                        pathMap.emplace(info.path, info.uuid);
                        idMap.emplace(info.uuid, pInfo);
                    }
                }

                json.End();
            }
        }

        for (const auto &relative : std::get<std::vector<FilePath>>(scanned)) {
            RegisterAsset(relative, false);
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
        for (const auto &relative : std::get<std::vector<FilePath>>(scanned)) {
            RegisterAsset(relative, false);
        }
        return {};
    }

    FileScanResult AssetDataBase::ScanSources() const
    {
        auto extensions = AssetBuilderManager::Get()->GetExtensions();
        extensions.push_back(".world");

        // Complete every mount before publishing assets or seeding manifests.
        std::vector<FilePath> files;
        for (const auto &fs : mountFsList) {
            if (fs == nullptr) {
                continue;
            }
            auto scanned = NativeFileSystem::FilterFiles(fs->GetPath(), extensions);
            if (const auto *error = std::get_if<FileScanError>(&scanned)) {
                return *error;
            }
            auto &paths = std::get<std::vector<FilePath>>(scanned);
            files.insert(files.end(), std::make_move_iterator(paths.begin()), std::make_move_iterator(paths.end()));
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

        auto fs = ResolveOwningFs(src->path);
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

        const int index = FindMountIndex(src->path);
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
        manifests.Invalidate(fs, dir);
        auto        manifest = manifests.Get(fs, dir);
        const auto *entry    = manifest.Find(fileName);
        if (entry == nullptr) {
            return false; // no manifest identity to attach the cook block to
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
        const int index = FindMountIndex(src->path);
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
