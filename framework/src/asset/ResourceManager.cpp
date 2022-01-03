//
// Created by Zach Lee on 2021/12/21.
//

#include <framework/asset/ResourceManager.h>

namespace sky {

    ResourceInstance ResourceManager::GetOrCreate(AssetBase& asset, const Uuid& id)
    {
        auto iter = resourceMap.find(id);
        if (iter != resourceMap.end()) {
            return iter->second;
        }

        auto res = asset.CreateInstance(id);
        if (!res) {
            return ResourceInstance {};
        }
        res->asset = &asset;
        resourceMap.emplace(id, res.Get());
        return res;
    }

    void ResourceManager::DestroyInstance(const Uuid& id)
    {
        auto iter = resourceMap.find(id);
        if (iter != resourceMap.end()) {
            delete iter->second;
            resourceMap.erase(iter);
        }
    }

}