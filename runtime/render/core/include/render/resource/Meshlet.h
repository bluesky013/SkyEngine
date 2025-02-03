//
// Created by blues on 2025/1/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

namespace sky {

    struct MeshletBound {
        Vector4 center; // 0-2: center 3: radius
        Vector4 coneApex;
        Vector4 coneAxis;
        Vector4 padding;
    };

    struct Meshlet {
        uint32_t vertexOffset;
        uint32_t triangleOffset;
        uint32_t vertexCount;
        uint32_t triangleCount;

        MeshletBound bounds;
    };

} // namespace sky