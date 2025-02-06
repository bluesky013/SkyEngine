//
// Created by Zach Lee on 2024/12/20.
//

#pragma once

#include <vector>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Meshlet.h>

namespace sky::builder {

    struct MeshMeshletsData {
        std::vector<Meshlet>   meshlets;
        std::vector<uint32_t>  meshletVertices;
        std::vector<uint32_t>  meshletTriangles;
    };

    class ClusterMeshBuilder {
    public:
        ClusterMeshBuilder();
        ~ClusterMeshBuilder() = default;

        static void BuildFromMeshData(MeshAssetData &data);
    };
} // namespace sky::builder
