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

        void RegisterHandler(const Uuid& type, AssetHandlerBase*);

        void UnRegisterHandler(const Uuid& type);

        template <typename T>
        void RegisterHandler()
        {
            RegisterHandler(T::TYPE, new AssetHandler<T>());
        }

        template <typename T>
        void UnRegisterHandler()
        {
            UnRegisterHandler(T::TYPE);
        }

        template <typename T>
        Asset<T> CreateAsset(const Uuid& id)
        {
            Asset<T> res(id);
            res.instance = static_cast<T*>(FindOrCreate(id, T::TYPE));
            return res;
        }

        void DestroyAsset(const Uuid& id);

    private:
        AssetInstanceBase* FindOrCreate(const Uuid&, const Uuid&);

        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();

        std::unordered_map<Uuid, AssetHandlerBase*> handlers;

        mutable std::mutex mutex;
        std::unordered_map<Uuid, AssetInstanceBase*> instances;
    };

}