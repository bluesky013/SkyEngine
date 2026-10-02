//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/serialization/JsonArchive.h>
#include <core/file/FileUtil.h>
#include <core/file/MultiFileSystem.h>
#include <core/logger/Logger.h>
#include <core/hash/Hash.h>

#include <fstream>
#include <iterator>

static const char* TAG = "AssetDataBase";

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
        auto mounts = new MultiFileSystem();
        if (workSpaceFs != nullptr) {
            mounts->AddFileSystem(workSpaceFs);
        }
        if (engineFs != nullptr) {
            mounts->AddFileSystem(engineFs);
        }
        mountFs = mounts;
    }

    FileSystemPtr AssetDataBase::ResolveOwningFs(const FilePath &path) const
    {
        if (workSpaceFs != nullptr && workSpaceFs->FileExist(path)) {
            return workSpaceFs;
        }
        if (engineFs != nullptr && engineFs->FileExist(path)) {
            return engineFs;
        }
        return {};
    }

    Uuid AssetDataBase::CalculateUuidByPath(const FilePath& path)
    {
        return Uuid::CreateWithSeed(Fnv1a32(path.GetStr()));
    }

    AssetSourcePtr AssetDataBase::FindAsset(const Uuid &id)
    {
        std::lock_guard lock(assetMutex);
        auto iter = idMap.find(id);
        return iter != idMap.end() ? iter->second : nullptr;
    }

    AssetSourcePtr AssetDataBase::FindAsset(const FilePath &path)
    {
        {
            std::lock_guard lock(assetMutex);
            auto iter = pathMap.find(path);
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

    std::vector<AssetSourcePtr> AssetDataBase::Gather(const std::string_view& category)
    {
        std::vector<AssetSourcePtr> res;

        std::lock_guard lock(assetMutex);
        for (auto& [id, asset] : idMap) {
            if (QueryType(asset->ext) == category) {
                res.emplace_back(asset);
            }
        }

        return res;
    }

    std::string AssetDataBase::QueryType(const std::string &ext) const
    {
        auto *builder = AssetBuilderManager::Get()->QueryBuilder(ext);
        return builder != nullptr ? std::string(builder->QueryType(ext)) : std::string{};
    }

    void AssetDataBase::EnsureManifestEntry(const FileSystemPtr &fs, const FilePath &path, const Uuid &uuid)    {
        const auto dir = path.Parent();
        const auto fileName = path.FileName();

        auto manifest = manifests.Get(fs, dir);
        if (manifest.Find(fileName) != nullptr) {
            return;
        }

        IndexFileEntry entry;
        entry.key = fileName;
        entry.id = uuid;
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
        out = AssetBuilderManager::Get()->GetCookConfig().ResolveTarget(CookJsonFor(src));
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
            auto iter = pathMap.find(path);
            if (iter != pathMap.end()) {
                info = idMap.at(iter->second);
            }
        }

        if (info != nullptr) {
            // Legacy asset (assets.db row): keep its identity and seed the manifest on first write.
            EnsureManifestEntry(fs, path, info->uuid);
        } else {
            const auto dir = path.Parent();
            const auto fileName = path.FileName();
            const auto *entry = manifests.Get(fs, dir).Find(fileName);

            const Uuid uuid = entry != nullptr ? entry->id : Uuid::Create();
            auto ext = path.Extension();

            AssetSourcePtr srcInfo = new AssetSourceInfo();
            srcInfo->path = path;
            srcInfo->uuid = uuid;
            srcInfo->ext = ext;

            {
                std::lock_guard lock(assetMutex);
                info = idMap.emplace(uuid, std::move(srcInfo)).first->second;
                pathMap.emplace(path, uuid);
            }

            if (entry == nullptr) {
                EnsureManifestEntry(fs, path, uuid);
            }
        }

        if (build) {
            AssetBuildRequest request = {};
            request.file = fs->OpenFile(path);
            request.assetInfo = info;
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
            auto iter = idMap.find(id);
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

        Uuid uuid;
        std::string cook;
        {
            const auto &manifest = manifests.Get(fs, from.Parent());
            const auto *entry = manifest.Find(from.FileName());
            uuid = entry != nullptr ? entry->id : Uuid::Create();
            cook = entry != nullptr ? entry->extra : std::string{};

            auto updated = manifest;
            updated.Remove(from.FileName());
            manifests.Save(fs, from.Parent(), updated);
        }
        {
            auto dest = manifests.Get(fs, to.Parent());
            IndexFileEntry entry;
            entry.key = to.FileName();
            entry.id = uuid;
            entry.extra = cook;
            dest.Set(entry);
            manifests.Save(fs, to.Parent(), dest);
        }

        AssetSourcePtr info;
        {
            std::lock_guard lock(assetMutex);
            auto iter = pathMap.find(from);
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

    void AssetDataBase::SetMarkedName(const Uuid& id, const std::string &name)
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

        auto archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);

        {
            uint32_t count = json.StartArray("assets");
            for (uint32_t i = 0; i < count; ++i) {
                auto *pInfo = new AssetSourceInfo();
                auto &info = *pInfo;

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

    void AssetDataBase::Save()
    {
        AssetExecutor::Get()->WaitForAll();

        if (workSpaceFs == nullptr) {
            return;
        }

        auto file = workSpaceFs->CreateOrOpenFile("assets.db");
        auto archive = file->WriteAsArchive();
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
        if (workSpaceFs == nullptr) {
            return;
        }

        const auto root = workSpaceFs->GetPath();
        for (const auto &ext : AssetBuilderManager::Get()->GetExtensions()) {
            for (const auto &relative : NativeFileSystem::FilterFiles(root, ext)) {
                RegisterAsset(FilePath{ relative }, false);
            }
        }
    }

    void AssetDataBase::BuildAllTargets(const Uuid &uuid)
    {
        auto src = FindAsset(uuid);
        if (src == nullptr) {
            return;
        }

        for (const auto &target : AssetBuilderManager::Get()->GetCookConfig().GetTargets(CookJsonFor(src))) {
            AssetBuilderManager::Get()->BuildRequest(uuid, target);
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

    void AssetDataBase::Dump(std::ostream &stream)
    {
        std::lock_guard lock(assetMutex);
        for (auto &[id, info] : idMap) {
            stream << id.ToString() << "\t"
                << QueryType(info->ext) << "\t"
                << info->name << "\t"
                << info->path.GetStr() << "\n";
        }
    }

} // namespace sky
