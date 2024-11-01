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
        ~RecastNaviMeshGenerator() override = default;

        void Setup(const WorldPtr &world) override;
    private:
        void GatherGeometry(NaviOctree* octree);
        void PrepareTiles(std::vector<RecastTile> &tiles) const;
        bool PrepareTileCache();
        bool BuildNavMesh();

        bool DoWork() override;
        void OnComplete(bool result) override;
        void PrepareWork() override;

        WorldPtr world;
        CounterPtr<RecastNaviMesh> navMesh;

        std::vector<RecastTile> pendingTiles;
        std::vector<CounterPtr<RecastTileGenerator>> tileGenerators;

        dtTileCache *tileCache = nullptr;

        rcConfig config;
    };

} // namespace sky::ai
