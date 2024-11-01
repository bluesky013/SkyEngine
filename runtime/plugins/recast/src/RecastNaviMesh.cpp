//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>
#include <recast/RecastNaviMeshGenerator.h>
#include <DetourNavMesh.h>

namespace sky::ai {

    RecastNavData::~RecastNavData()
    {
        if (data != nullptr) {
            dtFree(data);
        }
    }

    static void DebugDetourStatusDetail(dtStatus status)
    {
    }

    RecastNaviMesh::~RecastNaviMesh()
    {
        ResetNavMesh();
    }

    bool RecastNaviMesh::BuildNavMesh(const RecastNaviMapConfig &config)
    {
        navMesh = dtAllocNavMesh();

        dtNavMeshParams params = {};
        params.tileWidth  = resolution.tileSize;
        params.tileHeight = resolution.tileSize;
        params.maxTiles   = static_cast<int>(config.maxTiles);
        params.maxPolys   = static_cast<int>(config.maxPolys);

        auto status = navMesh->init(&params);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
            return false;
        }

        return true;
    }

    void RecastNaviMesh::ResetNavMesh()
    {
        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
        }
    }
} // namespace sky::ai