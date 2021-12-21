//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/Image.h>

namespace sky {

    AssetPtr ImageHandler::Create(const Uuid& id)
    {
        return new ImageAsset(id);
    }

    AssetPtr ImageHandler::Load(const std::string&)
    {
        return AssetPtr{};
    }

}