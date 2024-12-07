//
// Created by blues on 2024/11/7.
//

#pragma once

#include <cstdint>
#include <vector>
#include <core/util/Uuid.h>

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
    };

    struct TerrainQuad {
        TerrainSectionSize sectionSize = TerrainSectionSize::S63x63;
        float resolution = 1.f;
    };

    struct TerrainBuildConfig {
        TerrainSectionSize sectionSize = TerrainSectionSize::S63x63;
        float resolution = 1.f;

        int32_t sectionNumX = 8;
        int32_t sectionNumY = 8;

        Uuid material;
    };

    struct TerrainGenerateConfig {
        uint32_t seed = 1;
    };
} // namespace sky