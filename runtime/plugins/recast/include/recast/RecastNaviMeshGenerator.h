//
// Created by blues on 2024/9/6.
//

#pragma once

#include <Recast.h>
#include <DetourTileCache.h>
#include <navigation/NaviMeshGenerator.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastOctree.h>

namespace sky::ai {

    class RecastNaviMeshGenerator : public NaviMeshGenerator {
    public:
        RecastNaviMeshGenerator() = default;
        ~RecastNaviMeshGenerator() override = default;

        void Setup(const WorldPtr &world) override;
    private:
        void GatherGeometry(NaviOctree* octree);

        bool DoWork() override;
        void OnComplete(bool result) override;

        WorldPtr world;
        CounterPtr<RecastNaviMesh> navMesh;
        rcConfig config;
    };

} // namespace sky::ai
