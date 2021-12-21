//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/Material.h>

namespace sky {

    AssetPtr MaterialHandler::Create(const Uuid& id)
    {
        return new MaterialAsset(id);
    }

    AssetPtr MaterialHandler::Load(const std::string&)
    {
        return AssetPtr{};
    }

}