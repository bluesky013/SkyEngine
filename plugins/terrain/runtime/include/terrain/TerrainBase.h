//
// Created by blues on 2024/11/7.
//

#pragma once

#include <cstdint>
#include <vector>
#include <core/util/Uuid.h>
#include <core/name/Name.h>

namespace sky {

    enum class TerrainHeightFormat : uint8_t {
        R16_UNORM  = 0,
        R32_SFLOAT = 1,
    };

    struct ClipmapConfig {
        uint32_t blockSize    = 64;      // vertices per block side: 32, 64, 128, or 256
        uint32_t numLevels    = 8;       // number of clipmap LOD levels: 1-12
        float    resolution   = 1.0f;    // world-space meters per vertex at level 0
        float    heightScale  = 256.0f;  // texel value world height multiplier
        float    heightOffset = 0.0f;    // world height offset
        TerrainHeightFormat heightFormat = TerrainHeightFormat::R16_UNORM;
    };

    struct TerrainCoord {
        int32_t x;
        int32_t y;
    };

    struct TerrainVertex {
        uint8_t x;
        uint8_t y;
    };

    struct LayerInfo {
        Name name;
        Uuid albedo;
        Uuid normal;
        Uuid roughness;
    };

    struct TerrainGenerateConfig {
        uint32_t seed = 1;
    };
} // namespace sky