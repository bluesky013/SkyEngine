//
// Created by Zach on 2022/8/8.
//

#include <core/file/FileIO.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>

namespace sky {

    AssetManager::~AssetManager()
    {
        SaveAssets();
    }

    std::shared_ptr<AssetBase> AssetManager::CreateAsset(const Uuid &type, const Uuid &uuid)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return {};
        }
        auto assetHandler = hIter->second.get();
        auto asset = assetHandler->CreateAsset();
        asset->SetUuid(uuid);
        asset->status = AssetBase::Status::LOADING;
        {
            std::lock_guard<std::mutex> lock(assetMutex);
            assetMap.emplace(uuid, asset);
        }
        return asset;
    }

    std::string AssetManager::GetPathByUuid(const Uuid &id)
    {
        std::lock_guard<std::mutex> lock(dbMutex);
        auto iter = pathMap.find(id);
        if (iter != pathMap.end()) {
            return iter->second;
        }

        std::string result;
        dataBase->QueryProduct(id, result);
        if (!result.empty()) {
            pathMap.emplace(id, result);
        }
        return result;
    }

    std::shared_ptr<AssetBase> AssetManager::LoadAsset(const Uuid &type, const Uuid &uuid, bool async)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return {};
        }
        auto assetHandler = hIter->second.get();
        std::string path = GetPathByUuid(uuid);
        if (path.empty()) {
            return {};
        }

        std::shared_ptr<AssetBase> asset;
        {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto                        iter = assetMap.find(uuid);
            if (iter != assetMap.end()) {
                std::shared_ptr<AssetBase> res = iter->second.lock();
                if (res) {
                    return res;
                }
            }

            asset = assetHandler->CreateAsset();
            asset->SetUuid(uuid);
            asset->status = AssetBase::Status::LOADING;
            assetMap.emplace(uuid, asset);
        }

        auto fn = [asset, path, assetHandler]() {
            assetHandler->LoadFromPath(path, asset);
            asset->status = AssetBase::Status::LOADED;
        };

        if (async) {
            tf::Taskflow flow;
            flow.emplace(std::move(fn));
            asset->future = JobSystem::Get()->Run(std::move(flow));
        } else {
            fn();
        }
        return asset;
    }

    void AssetManager::RegisterBuilder(const std::string &key, AssetBuilder *builder)
    {
        if (builder == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(assetMutex);
        assetBuilders[key].emplace_back(std::move(builder));
    }

    void AssetManager::ImportSource(const std::string &path, const SourceAssetImportOption &option)
    {
        std::filesystem::path fs(path);
        auto ext = fs.extension().string();
        auto iter = assetBuilders.find(ext);

        BuildRequest request = {};
        request.fullPath = path;
        request.ext = ext;
        request.name = fs.filename().string();
        request.projectDir = Interface<ISystemNotify>::Get()->GetApi()->GetSettings().VisitString("PROJECT_PATH") + "/cache";

        BuildResult result = {};
        if (iter != assetBuilders.end()) {
            auto &builders = iter->second;
            for (auto &builder : builders) {
                builder->Request(request, result);
            }
        }
        {
            std::lock_guard<std::mutex> lock(dbMutex);
            for (auto &pdt : result.products) {
                dataBase->AddSource(SourceData{fs.make_preferred().string(), fs.parent_path().make_preferred().string(), pdt.productKey, pdt.uuid});
            }
        }
    }

    bool AssetManager::QueryOrImportSource(const std::string &path, const std::string &key, Uuid &out)
    {
        ImportSource(path, SourceAssetImportOption{key});
        return dataBase->QueryProduct(path, key, out);
    }

    void AssetManager::SaveAsset(const std::shared_ptr<AssetBase> &asset)
    {
        auto hIter = assetHandlers.find(asset->GetType());
        if (hIter == assetHandlers.end()) {
            return;
        }
        auto assetHandler = hIter->second.get();
        assetHandler->SaveToPath(asset->GetPath(), asset);
        dataBase->AddProduct({asset->GetUuid(), asset->GetPath()});
    }

    void AssetManager::RegisterSearchPath(const std::string &path)
    {
        searchPaths.emplace_back(path);
    }

    void AssetManager::RegisterAssetHandler(const Uuid &type, AssetHandlerBase *handler)
    {
        assetHandlers[type].reset(handler);
    }

    AssetHandlerBase *AssetManager::GetAssetHandler(const Uuid &type)
    {
        std::lock_guard<std::mutex> lock(assetMutex);
        auto iter = assetHandlers.find(type);
        if (iter == assetHandlers.end()) {
            return nullptr;
        }
        return iter->second.get();
    }

    std::string AssetManager::GetRealPath(const std::string &relative) const
    {
        std::filesystem::path path(relative);
        if (!std::filesystem::exists(path)) {
            for (auto &sp : searchPaths) {
                std::filesystem::path tmpPath(sp);
                tmpPath.append(path.string());
                if (std::filesystem::exists(tmpPath)) {
                    return tmpPath.make_preferred().string();
                }
            }
        }
        return relative;
    }

    void AssetManager::Reset(const std::string &name)
    {
        if (dataBase) {
            SaveAssets();
        }

        dataBase = std::make_unique<AssetDataBase>();
        dataBase->Init(name);
        RegisterAssets();
    }

    void AssetManager::SaveAssets()
    {

    }

    void AssetManager::RegisterAssets()
    {

    }

} // namespace sky