//
// Created by blues on 2025/2/9.
//

#pragma once

#include <render/resource/Meshlet.h>
#include <vector>

namespace sky::builder {

    struct MeshMeshletsData {
        std::vector<Meshlet>   meshlets;
        std::vector<uint32_t>  meshletVertices;
        std::vector<uint32_t>  meshletTriangles;
    };

    struct Cluster {
        std::vector<uint32_t> indices;
    };

} // namespace sky::builder
