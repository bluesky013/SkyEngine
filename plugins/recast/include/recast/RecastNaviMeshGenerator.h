//
// Created by blues on 2024/9/6.
//

#pragma once

#include <DetourTileCache.h>
#include <Recast.h>
#include <navigation/NaviMeshGenerator.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastTileGenerator.h>

namespace sky::ai {

    class RecastNaviMeshGenerator : public NaviMeshGenerator {
    public:
        RecastNaviMeshGenerator()           = default;
        ~RecastNaviMeshGenerator() override = default;

        void Setup(const WorldPtr &world) override;

        NaviMeshBuildParams GetBuildParams() const override;
        void                CollectTiles(NaviMeshData &out) const override;
        bool                HasSucceeded() const override
        {
            return succeeded.load();
        }
        std::string_view GetError() const override
        {
            return error;
        }

    private:
        void SnapshotTiles(NaviMeshData &out) const;
        bool GatherGeometry(NaviOctree *octree);
        void PrepareTiles(std::vector<RecastTile> &tiles) const;

        bool DoWork() override;
        void PrepareWork() override;

        WorldPtr                    world;
        CounterPtr<NaviMesh>        navMesh;
        std::shared_ptr<NaviOctree> buildOctree;
        NaviMeshBuildParams         buildParams;
        bool                        ready = false;
        std::atomic_bool            succeeded{false};
        std::string                 error;

        std::vector<RecastTile>                      pendingTiles;
        std::vector<CounterPtr<RecastTileGenerator>> tileGenerators;

        // Owns the exported bytes independently of Detour tile storage.
        NaviMeshData exportData;

        rcConfig config = {};
    };

} // namespace sky::ai
