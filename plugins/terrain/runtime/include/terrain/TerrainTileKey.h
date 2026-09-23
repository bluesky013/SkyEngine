//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

#include <cstddef>
#include <cstdint>

namespace sky::terrain {

    struct TerrainTileHash {
        size_t operator()(const TerrainTileCoord &coord) const
        {
            const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(coord.x)) << 32) |
                                 static_cast<uint32_t>(coord.y);
            return static_cast<size_t>(key ^ (key >> 32));
        }
    };

    struct TileLodKey {
        TerrainTileCoord coord;
        uint32_t         lod = 0;

        bool operator==(const TileLodKey &rhs) const { return lod == rhs.lod && coord == rhs.coord; }
        bool operator!=(const TileLodKey &rhs) const { return !(*this == rhs); }
    };

    struct TileLodHash {
        size_t operator()(const TileLodKey &key) const
        {
            const size_t h = TerrainTileHash{}(key.coord);
            return h ^ (static_cast<size_t>(key.lod) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
        }
    };

} // namespace sky::terrain
