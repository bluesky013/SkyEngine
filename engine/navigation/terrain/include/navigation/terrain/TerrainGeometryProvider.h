//
// Created on 2026/09/22.
//

#pragma once

#include <navigation/NaviGeometryProvider.h>

namespace sky::terrain {
    class ITerrainSystem;
} // namespace sky::terrain

namespace sky::ai {

    // Emits nav mesh geometry from terrain LOD0 samples (render/physics-free bridge).
    class TerrainGeometryProvider : public INaviGeometryProvider {
    public:
        explicit TerrainGeometryProvider(const terrain::ITerrainSystem *inSystem) : system(inSystem) {}
        ~TerrainGeometryProvider() override = default;

        bool Collect(const AABB &bounds, INaviGeometrySink &sink) override;

    private:
        const terrain::ITerrainSystem *system = nullptr;
    };

} // namespace sky::ai
