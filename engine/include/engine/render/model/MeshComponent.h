//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <engine/world/Component.h>
#include <engine/asset/MeshAsset.h>

namespace sky {

    class MeshComponent : public Component {
    public:
        MeshComponent() = default;
        ~MeshComponent() = default;

        TYPE_RTTI_WITH_VT(MeshComponent)

        static void Reflect();

        MeshAssetPtr asset;
    };

}