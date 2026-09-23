//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

#include <vector>

namespace sky::terrain {

    // Receives resident LOD0 geometry of a region. Heights are world heights, row-major (z * vertexSize + x).
    class ITerrainRegionSink {
    public:
        virtual ~ITerrainRegionSink() = default;

        virtual void OnTerrainTileLod0(const TerrainTileCoord &coord, const TerrainMeta &meta,
                                       const float *heights, uint32_t vertexSize) = 0;
    };

    // Notified when terrain tile data changes, so consumers (navigation, vegetation) can invalidate.
    class ITerrainChangeListener {
    public:
        virtual ~ITerrainChangeListener() = default;

        virtual void OnTerrainTilesChanged(const std::vector<TerrainTileCoord> &coords) = 0;
    };

} // namespace sky::terrain
