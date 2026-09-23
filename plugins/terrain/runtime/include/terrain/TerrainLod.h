//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

namespace sky::terrain {

    struct TerrainLodLevel {
        uint32_t level          = 0;
        float    scale          = 1.f;  // world meters per vertex at this level
        float    blockWorldSize = 0.f;  // tileSize * scale
        uint32_t lod            = 0;    // tile LOD this clipmap level samples
    };

    // Plain-data clipmap/LOD description; render layers consume it, the core owns no GPU state.
    struct TerrainLodDesc {
        uint32_t                     blockSize  = 64;
        float                        resolution = 1.f;
        std::vector<TerrainLodLevel> levels;

        uint32_t GetNumLevels() const { return static_cast<uint32_t>(levels.size()); }
    };

    TerrainLodDesc BuildTerrainLodDesc(const TerrainMeta &meta, uint32_t numLevels);

} // namespace sky::terrain
