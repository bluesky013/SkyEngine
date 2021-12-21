//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/Buffer.h>

namespace sky {

    AssetPtr BufferHandler::Create(const Uuid& id)
    {
        return new BufferAsset(id);
    }

    AssetPtr BufferHandler::Load(const std::string&)
    {
        return AssetPtr{};
    }

}