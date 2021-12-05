//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <framework/asset/Asset.h>
#include <unordered_map>
#include <mutex>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:

        template <typename T>
        void RegisterHandler()
        {
            RegisterHandler(Asset<T>::TYPE_ID, new AssetHandler<T>());
        }

        template <typename T>
        void UnRegisterHandler()
        {
            UnRegisterHandler(Asset<T>::TYPE_ID);
        }

        void RegisterHandler(uint32_t type, AssetHandlerBase*);

        void UnRegisterHandler(uint32_t type);

        template <typename T>
        Asset<T> CreateAsset(Uuid id, uint32_t type)
        {
            Asset<T> res(id);
            res.instance = FindOrCreate(id, type);
            return res;
        }

    private:
        AssetInstanceBase* FindOrCreate(Uuid, uint32_t);

        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();

        std::unordered_map<uint32_t, AssetHandlerBase*> handlers;

        mutable std::mutex mutex;
        std::unordered_map<Uuid, AssetInstanceBase*> instances;
    };

}