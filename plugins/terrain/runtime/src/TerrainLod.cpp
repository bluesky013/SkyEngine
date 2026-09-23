//
// Created on 2026/09/22.
//

#include <terrain/TerrainLod.h>

namespace sky::terrain {

    TerrainLodDesc BuildTerrainLodDesc(const TerrainMeta &meta, uint32_t numLevels)
    {
        TerrainLodDesc desc;
        desc.blockSize  = meta.tileSize;
        desc.resolution = meta.resolution;
        desc.levels.resize(numLevels);

        for (uint32_t level = 0; level < numLevels; ++level) {
            const float scale = meta.resolution * static_cast<float>(1u << level);
            desc.levels[level].level          = level;
            desc.levels[level].scale          = scale;
            desc.levels[level].blockWorldSize = static_cast<float>(meta.tileSize) * scale;
            desc.levels[level].lod            = meta.GetClampedLod(level);
        }

        return desc;
    }

} // namespace sky::terrain
