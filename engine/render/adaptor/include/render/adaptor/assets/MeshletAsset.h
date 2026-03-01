//
// Created by blues on 2025/1/3.
//

#pragma once

#include <cstdint>
#include <core/util/Uuid.h>

namespace sky {

    struct MeshletAssetData {
        uint32_t firstVertex   = 0;
        uint32_t firstTriangle = 0;
        uint32_t vertexCount   = 0;
        uint32_t triangleCount = 0;

        Uuid material;
    };

} // namespace sky