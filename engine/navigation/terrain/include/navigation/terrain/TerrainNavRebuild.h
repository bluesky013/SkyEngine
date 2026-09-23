//
// Created on 2026/09/23.
//

#pragma once

#include <framework/world/World.h>

#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshAsset.h>

#include <terrain/TerrainRegion.h>
#include <terrain/TerrainTypes.h>

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace sky::terrain {
    class ITerrainSystem;
} // namespace sky::terrain

namespace sky::ai {

    // Coordinates incremental nav mesh rebuilds from terrain changes:
    //  - maps changed terrain tiles to overlapping nav tiles
    //  - requires terrain LOD0 coverage for the region (defers otherwise)
    //  - rebuilds via the nav mesh generator's SetRebuildTiles (LOD0 only, never render LOD)
    class TerrainNavRebuildCoordinator : public terrain::ITerrainChangeListener {
    public:
        TerrainNavRebuildCoordinator()           = default;
        ~TerrainNavRebuildCoordinator() override;

        void Setup(terrain::ITerrainSystem *inTerrain, const NaviMeshBuildParams &inParams, const WorldPtr &inWorld);
        void Shutdown();

        // ITerrainChangeListener: queue overlapping nav tiles for rebuild.
        void OnTerrainTilesChanged(const std::vector<terrain::TerrainTileCoord> &coords) override;

        // Rebuilds pending nav tiles whose region has resident terrain LOD0; returns the count rebuilt.
        uint32_t Update();

        uint32_t GetPendingCount() const { return static_cast<uint32_t>(pendingTiles.size()); }
        const std::vector<NaviMeshTileCoord> &GetLastBuiltTiles() const { return lastBuiltTiles; }

    private:
        std::vector<NaviMeshTileCoord> MapTerrainTile(const terrain::TerrainTileCoord &coord) const;
        bool                            HasLod0Coverage(const NaviMeshTileCoord &tile) const;

        terrain::ITerrainSystem *terrain = nullptr;
        NaviMeshBuildParams      navParams;
        WorldPtr                 world;

        std::unordered_set<uint64_t>   pendingTiles;
        std::vector<NaviMeshTileCoord> lastBuiltTiles;
    };

} // namespace sky::ai
