//
// Created by blues on 2024/9/6.
//

#pragma once

#include <Recast.h>
#include <DetourTileCache.h>
#include <navigation/NaviMeshGenerator.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastTileGenerator.h>

namespace sky::ai {

    class RecastNaviMeshGenerator : public NaviMeshGenerator {
    public:
        RecastNaviMeshGenerator() = default;
        ~RecastNaviMeshGenerator() override;

        void Setup(const WorldPtr &world) override;

        NaviMeshBuildParams GetBuildParams() const override;
        void CollectTiles(NaviMeshData &out) const override;

    private:
        void SnapshotTiles(NaviMeshData &out) const;
        void GatherGeometry(NaviOctree* octree);
        void PrepareTiles(std::vector<RecastTile> &tiles) const;
        bool PrepareTileCache();
        bool BuildNavMesh();

        bool DoWork() override;
        void PrepareWork() override;

        WorldPtr world;
        CounterPtr<RecastNaviMesh> navMesh;

        std::vector<RecastTile> pendingTiles;
        std::vector<CounterPtr<RecastTileGenerator>> tileGenerators;

        dtTileCache *tileCache = nullptr;

        // Snapshot of tile payloads taken before ownership is transferred to the tile cache.
        NaviMeshData exportData;

        rcConfig config = {};
    };

} // namespace sky::ai
