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

    void AssetManager::RegisterHandler(uint32_t type, AssetHandlerBase* handler)
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

    void AssetManager::UnRegisterHandler(uint32_t type)
    {
        auto iter = handlers.find(type);
        if (iter != handlers.end()) {
            delete iter->second;
            handlers.erase(iter);
        }
    }

}