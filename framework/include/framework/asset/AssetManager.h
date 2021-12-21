//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <framework/asset/Asset.h>
#include <core/platform/Platform.h>
#include <unordered_map>
#include <mutex>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        void RegisterHandler(const Uuid& type, AssetHandlerBase*);

        void UnRegisterHandler(const Uuid& type);

        template <typename T>
        void RegisterHandler(AssetHandlerBase* handler)
        {
            SKY_ASSERT(handler != nullptr);
            RegisterHandler(T::TYPE, handler);
        }

        template <typename T>
        void UnRegisterHandler()
        {
            UnRegisterHandler(T::TYPE);
        }

        AssetPtr LoadAsset(const std::string& path, const Uuid& type);

        AssetPtr FindOrCreate(const Uuid& id, const Uuid& type);

        template <typename T>
        AssetPtr FindOrCreate(const Uuid& id)
        {
            return FindOrCreate(id, T::TYPE);
        }

        void DestroyAsset(const Uuid& id);

    private:

        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();

        std::unordered_map<Uuid, AssetHandlerBase*> handlers;

        mutable std::mutex mutex;
        std::unordered_map<Uuid, AssetBase*> assets;
    };

}