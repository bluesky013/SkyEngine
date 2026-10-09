//
// Created by blues on 2024/6/16.
//

#include <core/file/FileUtil.h>
#include <core/file/MultiFileSystem.h>
#include <core/hash/Hash.h>
#include <core/logger/Logger.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <fstream>
#include <iterator>

static const char *TAG = "AssetDataBase";

namespace sky {

    void AssetDataBase::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        engineFs = CastPtr<NativeFileSystem>(fs->CreateSubSystem("assets", true));
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

    void AssetDataBase::MigrateLegacyIdentity()
    {
        // Seed legacy `(bundle, path)` UUIDs into every source manifest. Existing entries and any
        // `assets.db` rows win (idempotent); only directories containing assets get a manifest.
        migrationMode = true;
        RebuildCacheFromScan();
        migrationMode = false;
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
        return const_cast<AssetDataBase *>(this)->FindAsset(id) != nullptr;
    }

    bool AssetDataBase::GetType(const Uuid &id, std::string &out) const
    {
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(id);
        if (src == nullptr) {
            return false;
        }
        out = QueryType(src->ext);
        return !out.empty();
    }

    bool AssetDataBase::GetTarget(const Uuid &id, std::string &out) const
    {
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(id);
        out      = AssetBuilderManager::Get()->GetCookConfig().ResolveTarget(CookJsonFor(src));
        return true;
    }

    bool AssetDataBase::GetSourcePath(const Uuid &id, std::string &out) const
    {
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(id);
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

        Uuid        uuid;
        std::string cook;
        {
            const auto &manifest = manifests.Get(fs, from.Parent());
            const auto *entry    = manifest.Find(from.FileName());
            uuid                 = entry != nullptr ? entry->id : Uuid::Create();
            cook                 = entry != nullptr ? entry->extra : std::string{};

            auto updated = manifest;
            updated.Remove(from.FileName());
            manifests.Save(fs, from.Parent(), updated);
        }
        {
            auto           dest = manifests.Get(fs, to.Parent());
            IndexFileEntry entry;
            entry.key   = to.FileName();
            entry.id    = uuid;
            entry.extra = cook;
            dest.Set(entry);
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

        fs->Copy(from, to);
        return RegisterAsset(to, false); // assigns a new UUID and writes the manifest entry
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

    void AssetDataBase::SetMarkedName(const Uuid &id, const std::string &name)
    {
        auto iter = idMap.find(id);
        if (iter == idMap.end()) {
            return;
        }

        iter->second->name = name;
    }

    void AssetDataBase::Load()
    {
        if (workSpaceFs == nullptr) {
            return;
        }

        auto file = workSpaceFs->OpenFile("assets.db");
        if (!file) {
            // The cache is regenerable; rebuild it from the writable source mount.
            RebuildCacheFromScan();
            return;
        }

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

        // Discover sources from every mount (the manifest provides stable identity; the db is only
        // a legacy seed / cross-check). Ensures read-only engine assets appear too.
        RebuildCacheFromScan();
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
        manifests.Clear();
    }

    void AssetDataBase::RebuildCacheFromScan()
    {
        // Discover sources across every mount in order (earlier mounts shadow later ones), with one
        // recursive walk per mount testing builder-known extensions in the visitor. Read-only mounts
        // keep path-derived identity and are not written to (except during the legacy migration, which
        // seeds the committed engine manifests).
        //
        // World documents (`.world`) are source assets too: they have no builder (never cooked), but they
        // get manifest identity like any other source so document/world references stay stable.
        auto extensions = AssetBuilderManager::Get()->GetExtensions();
        extensions.push_back(".world");

        for (const auto &fs : mountFsList) {
            if (fs == nullptr) {
                continue;
            }
            const auto root = fs->GetPath();
            for (const auto &relative : NativeFileSystem::FilterFiles(root, extensions)) {
                RegisterAsset(FilePath{relative}, false);
            }
        }
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
        auto src = const_cast<AssetDataBase *>(this)->FindAsset(id);
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

        auto        manifest = manifests.Get(fs, dir);
        const auto *entry    = manifest.Find(fileName);
        if (entry == nullptr) {
            return false; // no manifest identity to attach the cook block to
        }

        IndexFileEntry updated = *entry;
        updated.extra          = cookJson;
        manifest.Set(updated);
        manifests.Save(fs, dir, manifest);
        return true;
    }

    bool AssetDataBase::GetAbsoluteSourcePath(const Uuid &id, std::string &out) const
    {
        auto *self = const_cast<AssetDataBase *>(this);
        auto  src  = self->FindAsset(id);
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
