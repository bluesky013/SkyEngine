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

    AssetPtr AssetManager::LoadAsset(const std::string& path, const Uuid& type)
    {
        auto handler = handlers.find(type);
        if (handler == handlers.end()) {
            LOG_E(TAG, "handler not found %s", type.ToString().c_str());
            return {};
        }
        return handler->second->Load(path);
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
        assets.emplace(id, asset.Get());
        return asset;
    }

}