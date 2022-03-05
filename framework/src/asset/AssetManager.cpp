//
// Created by Zach Lee on 2021/12/4.
//

#include <framework/asset/AssetManager.h>
#include <core/logger/Logger.h>

static const char* TAG = "AssetManager";

namespace sky {

    AssetManager::AssetManager()
    {

    }

    AssetManager::~AssetManager()
    {

    }

    void AssetManager::UnRegisterHandler(const Uuid& type)
    {
        auto iter = handlers.find(type);
        if (iter != handlers.end()) {
            handlers.erase(iter);
        }
    }

    void AssetManager::SaveAsset(const std::string& path, AssetPtr asset, const Uuid& type)
    {
        auto handler = handlers.find(type);
        if (handler == handlers.end()) {
            LOG_E(TAG, "handler not found %s", type.ToString().c_str());
            return;
        }
        return handler->second->SaveAsset(asset.Get(), path);
    }

    void AssetManager::DestroyAsset(const Uuid& id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assets.find(id);
        if (iter != assets.end()) {
            if (!iter->second->path.empty()) {
                assetsFileMap.erase(iter->second->path);
            }
            delete iter->second;
            assets.erase(iter);
        }
    }

    AssetPtr AssetManager::FindOrCreate(const std::string& path, const Uuid& type)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assetsFileMap.find(path);
        if (iter != assetsFileMap.end()) {
            return FindOrCreate(iter->second, type);
        }

        auto handler = handlers.find(type);
        if (handler == handlers.end()) {
            LOG_E(TAG, "handler not found %s", type.ToString().c_str());
            return {};
        }
        auto asset = handler->second->Load(path);
        if (asset != nullptr) {
            asset->path = path;
            asset->status = AssetBase::Status::LOADED;
            assetsFileMap.emplace(path, asset->GetId());
        }
        return asset;
    }

    AssetPtr AssetManager::FindOrCreate(const Uuid& id, const Uuid& type)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iIter = assets.find(id);
        if (iIter != assets.end()) {
            return iIter->second;
        }

        auto tIter = handlers.find(type);
        if (tIter == handlers.end()) {
            LOG_E(TAG, "handler not found %s", type.ToString().c_str());
            return {};
        }
        auto asset = tIter->second->Create(id);
        assets.emplace(id, asset);
        return asset;
    }

}