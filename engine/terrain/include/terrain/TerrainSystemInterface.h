//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainQuery.h>
#include <terrain/TerrainRegion.h>
#include <terrain/TerrainTypes.h>

#include <core/shapes/AABB.h>

#include <cstdint>
#include <vector>

namespace sky::terrain {

    // Engine-side terrain sub-system interface (implementation lives in a plugin). Exposes only the
    // surface that engine consumers (e.g. navigation/vegetation bridges) need.
    class ITerrainSystem {
    public:
        virtual ~ITerrainSystem() = default;

        virtual const ITerrainField &GetField() const = 0;
        virtual const TerrainMeta &GetMeta() const = 0;
        virtual const TerrainTileManifest &GetManifest() const = 0;
        virtual uint32_t GetLoadedTileCount() const = 0;

        virtual bool SampleRegionLod0(const AABB &bounds, ITerrainRegionSink &sink) const = 0;

        virtual void AddChangeListener(ITerrainChangeListener *listener) = 0;
        virtual void RemoveChangeListener(ITerrainChangeListener *listener) = 0;
        virtual void NotifyTilesChanged(const std::vector<TerrainTileCoord> &coords) = 0;
    };

} // namespace sky::terrain
