//
// Created by Zach Lee on 2021/12/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/Resource.h>
#include <framework/asset/Asset.h>

namespace sky {

    class ResourceManager : public Singleton<ResourceManager> {
    public:
        ResourceInstance GetOrCreate(AssetBase& asset, const Uuid& id);

        template <typename T>
        CounterPtr<T> GetOrCreate(const Uuid& id);

        void DestroyInstance(const Uuid& id);

    private:
        friend class Singleton<ResourceManager>;
        ResourceManager() = default;
        ~ResourceManager() = default;

        using ResourceMap = std::unordered_map<Uuid, ResourceBase*>;
        ResourceMap resourceMap;
    };

    template <typename T>
    CounterPtr<T> ResourceManager::GetOrCreate(const Uuid& id)
    {
        auto iter = resourceMap.find(id);
        if (iter != resourceMap.end()) {
            return iter->second;
        }
        T* res = new T(id);
        res->asset = nullptr;
        resourceMap.emplace(id, res);
        return res;
    }

}