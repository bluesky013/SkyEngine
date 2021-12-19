//
// Created by Zach Lee on 2021/12/4.
//

#include <framework/asset/AssetManager.h>

namespace sky {

    AssetManager::AssetManager()
    {

    }

    AssetManager::~AssetManager()
    {

    }

    void AssetManager::RegisterHandler(const Uuid& type, AssetHandlerBase* handler)
    {
        if (handler == nullptr) {
            return;
        }
        auto iter = handlers.find(type);
        if (iter != handlers.end()) {
            if (iter->second == handler) {
                return;
            }
            delete iter->second;
        }
        handlers.emplace(type, handler);
    }

    void AssetManager::UnRegisterHandler(const Uuid& type)
    {
        auto iter = handlers.find(type);
        if (iter != handlers.end()) {
            delete iter->second;
            handlers.erase(iter);
        }
    }

    void AssetManager::DestroyAsset(const Uuid& id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = assets.find(id);
        if (iter != assets.end()) {
            delete iter->second;
            assets.erase(iter);
        }
    }

    AssetDataBase* AssetManager::FindOrCreate(const Uuid& id, const Uuid& type)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iIter = assets.find(id);
        if (iIter != assets.end()) {
            return iIter->second;
        }

        auto tIter = handlers.find(type);
        if (tIter == handlers.end()) {
            return nullptr;
        }
        auto asset = tIter->second->Create(id);
        assets.emplace(id, asset);
        return asset;
    }

}