//
// Created by Zach on 2022/8/8.
//

#include <core/file/FileIO.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>

namespace sky {

    std::shared_ptr<AssetBase> AssetManager::GetOrCreate(const Uuid &type, const Uuid &uuid, bool async)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return {};
        }
        auto assetHandler = hIter->second.get();

        auto pIter = pathMap.find(uuid);
        if (pIter == pathMap.end()) {
            return {};
        }
        std::string path = pIter->second;

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

    void AssetManager::SaveAsset(const std::shared_ptr<AssetBase> &asset, const Uuid &type, const std::string &path)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return;
        }
        auto assetHandler = hIter->second.get();
        assetHandler->SaveToPath(path, asset);
    }

    void AssetManager::RegisterAsset(const Uuid &id, const std::string &path)
    {
        pathMap[id] = path;
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
                    return tmpPath.string();
                }
            }
        }
        return relative;
    }
} // namespace sky