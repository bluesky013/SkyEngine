//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/task/TaskManager.h>
#include <core/environment/Singleton.h>
#include <core/platform/Platform.h>
#include <unordered_map>
#include <mutex>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        void UnRegisterHandler(const Uuid& type);

        template <typename T>
        void RegisterHandler()
        {
            handlers[T::TYPE].reset(new AssetHandler<T>());
        }

        template <typename T>
        void UnRegisterHandler()
        {
            UnRegisterHandler(T::TYPE);
        }

        void SaveAsset(const std::string& path, AssetPtr asset, const Uuid& type);

        AssetPtr FindOrCreate(const std::string& path, const Uuid& type);
        AssetPtr FindOrCreate(const Uuid& id, const Uuid& type);

        template <typename T>
        CounterPtr<T> FindOrCreate(const Uuid& id)
        {
            return Cast<T>(FindOrCreate(id, T::TYPE));
        }

        void DestroyAsset(const Uuid& id);

    private:

        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();

        using AssetHanlderPtr = std::unique_ptr<AssetHandlerBase>;

        std::unordered_map<Uuid, AssetHanlderPtr> handlers;

        mutable std::mutex mutex;
        std::unordered_map<Uuid, AssetBase*> assets;
        std::unordered_map<std::string, Uuid> assetsFileMap;
    };

}