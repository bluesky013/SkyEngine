//
// Created by blues on 2024/11/7.
//

#pragma once

#include <cstdint>
#include <vector>

namespace sky {

    enum class TerrainSectionSize : uint8_t {
        S31x31   = 0,
        S63x63   = 1,
        S127x127 = 2,
    };

    struct TerrainConfig {
        uint32_t maxExt  = 16 * 1024;
        uint32_t leafExt = 64;
        float resolution = 0.5f;
    };

    struct TerrainCoord {
        int32_t x;
        int32_t y;
    };

    struct TerrainVertex {
        uint8_t x;
        uint8_t y;
        uint8_t subx;
        uint8_t suby;
    };

    struct TerrainQuad {
        uint8_t            subSection  = 2;
        TerrainSectionSize sectionSize = TerrainSectionSize::S63x63;
    };
} // namespace sky