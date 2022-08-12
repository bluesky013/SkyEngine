//
// Created by yjrj on 2022/8/8.
//

#include <framework/asset/AssetManager.h>

namespace sky {

    void AssetManager::RegisterAsset(const Uuid& id, const std::string& path)
    {
        pathMap[id] = path;
    }

}