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
        void RegisterHandler(uint32_t type, AssetHandlerBase*);

        void UnRegisterHandler(uint32_t type);


    private:
        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();

        std::unordered_map<uint32_t, AssetHandlerBase*> handlers;

        mutable std::mutex mutex;
        std::unordered_map<Uuid, AssetInstanceBase*> assets;
    };

}