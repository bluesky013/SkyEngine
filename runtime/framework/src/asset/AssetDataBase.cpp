//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/serialization/JsonArchive.h>
#include <core/file/FileUtil.h>
#include <core/logger/Logger.h>

static const char* TAG = "AssetDataBase";

namespace sky {

    void AssetDataBase::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        engineFs = fs->CreateSubSystem("assets", true);

        AssetBuilderManager::Get()->SetEngineFs(engineFs);
    }

    void AssetDataBase::SetWorkSpaceFs(const NativeFileSystemPtr &fs)
    {
        workSpaceFs = fs->CreateSubSystem("assets", true);

        AssetBuilderManager::Get()->SetWorkSpaceFs(fs);
    }

    AssetSourcePtr AssetDataBase::FindAsset(const Uuid &id)
    {
        std::lock_guard<std::recursive_mutex> lock(assetMutex);
        auto iter = idMap.find(id);
        return iter != idMap.end() ? iter->second : nullptr;
    }

    AssetSourcePtr AssetDataBase::FindAsset(const std::string &path)
    {
        std::lock_guard<std::recursive_mutex> lock(assetMutex);
        const SourceAssetBundle bundles[] = {
                SourceAssetBundle::WORKSPACE,
                SourceAssetBundle::ENGINE
        };
        for (const auto &bundle : bundles) {
            auto iter = pathMap.find({bundle, path});
            if (iter != pathMap.end()) {
                return FindAsset(iter->second);
            }
        }
        return {};
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const AssetSourcePath &path)
    {
        const auto &fs = GetFileSystemBySourcePath(path);
        if (!fs->FileExist(path.path)) {
            LOG_E(TAG, "File not Exist %s", path.path.GetStr().c_str());
            return nullptr;
        }

        AssetSourcePtr info = nullptr;
        {
            std::lock_guard<std::recursive_mutex> lock(assetMutex);
            auto iter = pathMap.find(path);
            if (iter != pathMap.end()) {
                info = idMap.at(iter->second);
            }
        }

        if (info == nullptr) {
            // query builder
            auto ext = path.path.Extension();
            auto *builder = AssetBuilderManager::Get()->QueryBuilder(ext);
            if (builder == nullptr) {
                LOG_E(TAG, "Builder not found for asset %s", ext.c_str());
                return nullptr;
            }

            AssetSourcePtr srcInfo = new AssetSourceInfo();
            auto uuid = Uuid::Create();
            srcInfo->path = path;
            srcInfo->uuid = uuid;
            srcInfo->ext = ext;
            srcInfo->category = builder->QueryType(ext);

            // new asset
            {
                std::lock_guard<std::recursive_mutex> lock(assetMutex);
                pathMap.emplace(path, uuid);
                info = idMap.emplace(uuid, std::move(srcInfo)).first->second;
            }
        }

        AssetBuildRequest request = {};
        request.file = fs->OpenFile(path.path);
        request.assetInfo = info;
        AssetBuilderManager::Get()->BuildRequest(request);

        // request builder
        return info;
    }

    AssetSourcePtr AssetDataBase::RegisterAsset(const std::string &path)
    {
        auto querySource = QuerySource(path);
        if (querySource.bundle == SourceAssetBundle::INVALID) {
            return nullptr;
        }

        return RegisterAsset(querySource);
    }

    void AssetDataBase::RemoveAsset(const Uuid &id)
    {
        std::lock_guard<std::recursive_mutex> lock(assetMutex);
        SKY_ASSERT(idMap.count(id));
        auto &src = idMap[id];
        pathMap.erase(src->path);
        idMap.erase(id);
    }

    FilePtr AssetDataBase::OpenFile(const AssetSourcePtr &src)
    {
        const auto &fs = GetFileSystemBySourcePath(src->path);
        return fs->OpenFile(src->path.path);
    }

    FilePtr AssetDataBase::CreateOrOpenFile(const AssetSourcePath &path)
    {
        const auto &fs = GetFileSystemBySourcePath(path);
        return fs->CreateOrOpenFile(path.path);
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
        auto file = workSpaceFs->OpenFile("assets.db");
        if (!file) {
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

                json.Start("type");
                info.category = json.LoadString();
                json.End();

                json.Start("bundle");
                info.path.bundle = static_cast<SourceAssetBundle>(json.LoadUint());
                json.End();

                json.Start("path");
                info.path.path = json.LoadString();
                json.End();

                uint32_t depCount = json.StartArray("dependencies");
                for (uint32_t j = 0; j < depCount; ++j) {
                    info.dependencies.emplace_back(Uuid::CreateFromString(json.LoadString()));
                    json.NextArrayElement();
                }
                json.End();

                json.NextArrayElement();

                {
                    std::lock_guard<std::recursive_mutex> lock(assetMutex);
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

        auto file = workSpaceFs->CreateOrOpenFile("assets.db");
        auto archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);

        {
            std::lock_guard<std::recursive_mutex> lock(assetMutex);

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

                json.Key("type");
                json.SaveValue(info.category);

                json.Key("bundle");
                json.SaveEnum(info.path.bundle);

                json.Key("path");
                json.SaveValue(info.path.path.GetStr());

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
        std::lock_guard<std::recursive_mutex> lock(assetMutex);
        pathMap.clear();
        idMap.clear();
    }

    void AssetDataBase::Dump(std::ostream &stream)
    {
        std::lock_guard<std::recursive_mutex> lock(assetMutex);
        for (auto &[id, info] : idMap) {
            stream << id.ToString() << "\t"
                << info->category << "\t"
                << info->name << "\t"
                << "@" << static_cast<uint32_t>(info->path.bundle) << "@" << info->path.path.GetStr() << "\n";
        }
    }

    AssetSourcePath AssetDataBase::QuerySource(const std::string &path)
    {
        if (workSpaceFs->FileExist(FilePath(path))) {
            return {SourceAssetBundle::WORKSPACE, path};
        }

        if (engineFs->FileExist(FilePath(path))) {
            return {SourceAssetBundle::ENGINE, path};
        }

        return {SourceAssetBundle::INVALID};
    }

    const NativeFileSystemPtr &AssetDataBase::GetFileSystemBySourcePath(const AssetSourcePath &path)
    {
        static NativeFileSystemPtr empty;
        switch (path.bundle) {
            case SourceAssetBundle::ENGINE:
                return engineFs;
            case SourceAssetBundle::WORKSPACE:
                return workSpaceFs;
            case SourceAssetBundle::INVALID:
            case SourceAssetBundle::CUSTOM_BEGIN:
                break;
        }
        return empty;
    }
} // namespace sky