//
// Created by Zach Lee on 2022/8/25.
//

#include <cereal/archives/json.hpp>
#include <framework/asset/AssetManager.h>
#include <render/resources/Prefab.h>

namespace sky {

    std::shared_ptr<Prefab> Prefab::CreateFromData(const PrefabData &data)
    {
        return {};
    }

}