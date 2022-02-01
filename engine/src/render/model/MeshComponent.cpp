//
// Created by Zach Lee on 2022/1/31.
//

#include <engine/render/model/MeshComponent.h>

namespace sky {

    void MeshComponent::OnTick(float time)
    {
        if (asset && !instance) {
            instance = Mesh::CreateFromAsset(asset);
        }
    }
}