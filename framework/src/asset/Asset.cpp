//
// Created by Zach Lee on 2021/12/21.
//

#include <framework/asset/Asset.h>
#include <framework/asset/AssetManager.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

namespace sky {

    void AssetBase::OnExpire()
    {
        AssetManager::Get()->DestroyAsset(uuid);
    }

    AssetBase* AssetHandlerBase::Load(const std::string& path)
    {
        return Create(Uuid::Create());
    }

}