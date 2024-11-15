//
// Created by blues on 2024/11/7.
//

#pragma once

#include <cstdint>
#include <vector>

namespace sky {

    struct TerrainConfig {
        uint32_t maxExt  = 16 * 1024;
        uint32_t leafExt = 64;
        float resolution = 0.5f;
    };

    struct TerrainCoord {
        int32_t x;
        int32_t y;
    };


} // namespace sky