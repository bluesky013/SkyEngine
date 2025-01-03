//
// Created by Zach Lee on 2024/12/20.
//

#pragma once

#include <vector>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Meshlet.h>

namespace sky::builder {

    class ClusterMeshBuilder {
    public:
        ClusterMeshBuilder();
        ~ClusterMeshBuilder() = default;

        static void BuildFromMeshData(const MeshAssetData &data);
    };
} // namespace sky::builder
