//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <mutex>
#include <unordered_map>
#include <core/environment/Singleton.h>
#include <framework/asset/Asset.h>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        ~AssetManager() = default;

        template<class T>
        std::shared_ptr<Asset<T>> LoadAsset(const Uuid& uuid)
        {
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                auto iter = assetMap.find(uuid);
                if (iter != assetMap.end()) {
                    std::shared_ptr<AssetBase> res = iter->second.lock();
                    if (res) {
                        return res;
                    }
                }
            }

            std::shared_ptr<Asset<T>> asset = std::make_shared<Asset<T>>();
            asset->SetUuid(uuid);

            {
                std::lock_guard<std::mutex> lock(assetMutex);
            }
        }

        void RegisterAsset(const Uuid& id, const std::string& path);

    private:
        friend class Singleton<AssetManager>;
        AssetManager() = default;

//        std::unordered_map<uint32_t

        std::unordered_map<Uuid, std::string> pathMap;

        mutable std::mutex pendingMutex;

        mutable std::mutex assetMutex;
        std::unordered_map<Uuid, std::weak_ptr<AssetBase>> assetMap;
    };

}
