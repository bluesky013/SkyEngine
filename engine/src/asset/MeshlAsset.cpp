//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/MeshAsset.h>

namespace sky {

    AssetBase* MeshAssetHandler::Create(const Uuid& id)
    {
        return new MeshAsset(id);
    }

    AssetBase* MeshAssetHandler::Load(const std::string&)
    {
        return nullptr;
    }
}