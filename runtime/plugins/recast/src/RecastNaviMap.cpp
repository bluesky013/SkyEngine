//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMap.h>

namespace sky::ai {

    static void DebugDetourStatusDetail(dtStatus status)
    {
    }

    void RecastNaviMap::BuildNavMesh(const RecastNaviMapConfig &config)
    {
        navMesh = dtAllocNavMesh();

        dtNavMeshParams params = {};
        params.tileWidth  = config.tileWidth;
        params.tileHeight = config.tileHeight;
        params.maxTiles   = config.maxTiles;
        params.maxPolys   = config.maxPolys;

        auto status = navMesh->init(&params);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
            return;
        }
    }

    void RecastNaviMap::ResetNavMesh()
    {
        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
        }
    }

} // namespace sky::ai