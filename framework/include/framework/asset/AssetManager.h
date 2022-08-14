//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <mutex>
#include <unordered_map>
#include <core/environment/Singleton.h>
#include <core/jobsystem/JobSystem.h>
#include <framework/asset/Asset.h>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        ~AssetManager() = default;

        template<class T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid& uuid)
        {
            auto pIter = pathMap.find(uuid);
            if (pIter == pathMap.end()) {
                return {};
            }

            std::shared_ptr<Asset<T>> asset = GetOrCreate<T>(uuid);
            AssetTraits<T>::LoadFromPath(pIter->second, asset->data);
            asset->status = AssetBase::Status::LOADED;
            return asset;
        }

        template<class T>
        std::shared_ptr<Asset<T>> LoadAssetAsync(const Uuid& uuid)
        {
            auto pIter = pathMap.find(uuid);
            if (pIter == pathMap.end()) {
                return {};
            }

            std::shared_ptr<Asset<T>> asset = GetOrCreate<T>(uuid);
            asset->status = AssetBase::Status::LOADING;

            tf::Taskflow flow;
            std::string& path = pIter->second;
            flow.emplace([asset, path]() {
                AssetTraits<T>::LoadFromPath(path, asset->data);
                asset->status = AssetBase::Status::LOADED;
            });

            asset->future = JobSystem::Get()->Run(std::move(flow));
            return asset;
        }

        void RegisterAsset(const Uuid& id, const std::string& path);

        void RegisterSearchPath(const std::string& path);

        std::string GetRealPath(const std::string& relative) const;

    private:
        template <typename T>
        std::shared_ptr<Asset<T>> GetOrCreate(const Uuid uuid)
        {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto iter = assetMap.find(uuid);
            if (iter != assetMap.end()) {
                std::shared_ptr<AssetBase> res = iter->second.lock();
                if (res) {
                    return std::static_pointer_cast<Asset<T>>(res);
                }
            }
            auto asset = std::make_shared<Asset<T>>();
            asset->SetUuid(uuid);
            assetMap.emplace(uuid, asset);
            return asset;
        }


        friend class Singleton<AssetManager>;
        AssetManager() = default;

        std::unordered_map<Uuid, std::string> pathMap;
        std::vector<std::string> searchPaths;

        mutable std::mutex pendingMutex;

        mutable std::mutex assetMutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assetMap;
    };

}
