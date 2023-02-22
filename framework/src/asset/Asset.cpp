//
// Created by Zach on 2022/8/8.
//

#include <framework/asset/Asset.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void AssetBase::SetUuid(const Uuid &id)
    {
        uuid = id;
    }

    void AssetBase::SetPath(const std::string &fullPath)
    {
        path = fullPath;
    }

    std::string AssetHandlerBase::GetRealPath(const std::string &path)
    {
        return AssetManager::Get()->GetRealPath(path);
    }

} // namespace sky