//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMap.h>

namespace sky::ai {

    static void DebugDetourStatusDetail(dtStatus status)
    {
    }

    void RecastNaviMap::BuildNavMesh()
    {
        navMesh = dtAllocNavMesh();

        dtNavMeshParams params = {};
        params.tileWidth  = 12.8;
        params.tileHeight = 12.8;
        params.maxTiles   = 65536;
        params.maxPolys   = 65536;

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