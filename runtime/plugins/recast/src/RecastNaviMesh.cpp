//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>

namespace sky::ai {

    static void DebugDetourStatusDetail(dtStatus status)
    {
    }

    void RecastNaviMesh::BuildNavMesh(const RecastNaviMapConfig &config)
    {
        navMesh = dtAllocNavMesh();

        dtNavMeshParams params = {};
        params.tileWidth  = config.tileWidth;
        params.tileHeight = config.tileHeight;
        params.maxTiles   = static_cast<int>(config.maxTiles);
        params.maxPolys   = static_cast<int>(config.maxPolys);

        auto status = navMesh->init(&params);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
            return;
        }
    }

    void RecastNaviMesh::ResetNavMesh()
    {
        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
        }
    }

} // namespace sky::ai