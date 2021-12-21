//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/Model.h>

namespace sky {

    AssetPtr ModelHandler::Create(const Uuid& id)
    {
        return new ModelAsset(id);
    }

    AssetPtr Load(const std::string&)
    {
        return AssetPtr {};
    }
}