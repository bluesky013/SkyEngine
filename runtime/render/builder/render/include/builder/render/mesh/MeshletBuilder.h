//
// Created by Zach Lee on 2024/12/20.
//

#pragma once

#include <vector>
#include <render/adaptor/assets/MeshAsset.h>
#include <builder/render/mesh/ClusterBase.h>

namespace sky::builder {

    class MeshletBuilder {
    public:
        MeshletBuilder();
        ~MeshletBuilder() = default;

        static void BuildFromMeshData(MeshAssetData &data);
    };
} // namespace sky::builder
