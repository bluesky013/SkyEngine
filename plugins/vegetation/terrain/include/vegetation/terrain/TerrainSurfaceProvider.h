//
// Created on 2026/09/22.
//

#pragma once

#include <vegetation/VegetationSurface.h>

#include <terrain/TerrainRegion.h>

#include <vector>

namespace sky::terrain {
    class ITerrainSystem;
} // namespace sky::terrain

namespace sky::vegetation {

    // Bridges terrain (LOD0) into the vegetation surface provider seam. Cells align to terrain tiles;
    // terrain tile changes are forwarded as surface-change notifications.
    class TerrainSurfaceProvider : public IVegetationSurfaceProvider, public terrain::ITerrainChangeListener {
    public:
        explicit TerrainSurfaceProvider(terrain::ITerrainSystem *inSystem);
        ~TerrainSurfaceProvider() override;

        bool  SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const override;
        float GetCellSize() const override;
        void  GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const override;
        AABB  GetCellBounds(int32_t cellX, int32_t cellY) const override;

        void AddSurfaceListener(IVegetationSurfaceListener *listener) override;
        void RemoveSurfaceListener(IVegetationSurfaceListener *listener) override;

        // ITerrainChangeListener
        void OnTerrainTilesChanged(const std::vector<terrain::TerrainTileCoord> &coords) override;

    private:
        terrain::ITerrainSystem                  *system = nullptr;
        std::vector<IVegetationSurfaceListener *> listeners;
    };

} // namespace sky::vegetation
