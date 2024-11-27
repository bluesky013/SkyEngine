//
// Created by blues on 2024/11/26.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <render/resource/Texture.h>
#include <core/math/Vector4.h>
#include <core/shapes/AABB.h>

namespace sky {

    struct TerrainSector {
        TerrainCoord   coord;
        RDTexture2DPtr heightMap;
    };

    struct TerrainInstanceData {
        Vector4 worldOffset;
        Vector4 boxMin;
        Vector4 boxMax;
    };

} // namespace sky
