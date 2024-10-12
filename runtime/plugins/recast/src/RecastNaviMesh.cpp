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

    NaviMesh* RecastNaviMapFactory::CreateNaviMesh()
    {
        return new RecastNaviMesh();
    }

    NaviMeshGenerator* RecastNaviMapFactory::CreateGenerator()
    {
        return new RecastNaviMeshGenerator();
    }

} // namespace sky::ai