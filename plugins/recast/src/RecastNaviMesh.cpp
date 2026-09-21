//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>
#include <recast/RecastConstants.h>
#include <recast/RecastLz4Compressor.h>
#include <recast/RecastQueryFilter.h>
#include <recast/RecastDebugDraw.h>
#include <recast/RecastTileCacheMeshProcessor.h>

#include <navigation/NaviMeshAsset.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourTileCache.h>

#include <cstring>

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

    RecastNaviMesh::RecastNaviMesh() = default;

    RecastNaviMesh::~RecastNaviMesh()
    {
        ResetNavMesh();
    }

    bool RecastNaviMesh::BuildNavMesh(const RecastNaviMapConfig &config)
    {
        navMesh = dtAllocNavMesh();
        if (navMesh == nullptr) {
            return false;
        }

        dtNavMeshParams params = {};
        params.tileWidth  = resolution.tileSize;
        params.tileHeight = resolution.tileSize;
        params.maxTiles   = static_cast<int>(config.maxTiles);
        params.maxPolys   = static_cast<int>(config.maxPolys);

        auto status = navMesh->init(&params);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
            return false;
        }

        return true;
    }

    void RecastNaviMesh::BuildNavQuery()
    {
        if (navMesh == nullptr) {
            return;
        }

        navQuery = dtAllocNavMeshQuery();
        if (navQuery == nullptr) {
            return;
        }

        auto status = navQuery->init(navMesh, RECAST_MAX_QUERY_NODES);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
            dtFreeNavMeshQuery(navQuery);
            navQuery = nullptr;
        }
    }

    void RecastNaviMesh::BuildDebugGeometry(NaviDebugGeometry &out) const
    {
        if (navMesh == nullptr) {
            out.vertices.clear();
            return;
        }

        RecastBuildNavMeshGeometry(*navMesh, out);
    }

    void RecastNaviMesh::ResetNavMesh()
    {
        if (tileCache != nullptr) {
            dtFreeTileCache(tileCache);
            tileCache = nullptr;
        }

        if (navQuery != nullptr) {
            dtFreeNavMeshQuery(navQuery);
            navQuery = nullptr;
        }

        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
        }
    }

    bool RecastNaviMesh::LoadData(const NaviMeshData &data)
    {
        if (data.mode != NaviMeshExportMode::Tiled) {
            return false;
        }

        ResetNavMesh();

        agentCfg    = data.params.agent;
        resolution  = data.params.resolution;
        buildBounds = data.params.bounds;

        RecastNaviMapConfig navConfig = {};
        if (!BuildNavMesh(navConfig)) {
            return false;
        }

        static dtTileCacheAlloc           GAllocator;
        static RecastTileCacheMeshProcessor GMeshProcessor;

        tileCache = dtAllocTileCache();
        if (tileCache == nullptr) {
            ResetNavMesh();
            return false;
        }

        dtTileCacheParams tcParams = {};
        tcParams.cs = resolution.cellSize;
        tcParams.ch = resolution.cellHeight;
        tcParams.width  = resolution.cellSize > 0.f ? static_cast<int>(resolution.tileSize / resolution.cellSize) : 0;
        tcParams.height = tcParams.width;
        tcParams.maxSimplificationError = data.params.maxSimplificationError;
        tcParams.walkableHeight = agentCfg.height;
        tcParams.walkableRadius = agentCfg.radius;
        tcParams.walkableClimb  = agentCfg.maxClimb;
        tcParams.maxTiles       = RECAST_MAX_BUILD_TILES;
        tcParams.maxObstacles   = RECAST_MAX_OBSTACLES;

        if (dtStatusFailed(tileCache->init(&tcParams, &GAllocator, GetOrCreateCompressor(), &GMeshProcessor))) {
            ResetNavMesh();
            return false;
        }

        for (const auto &tile : data.tiles) {
            if (tile.data.empty()) {
                continue;
            }

            auto *buffer = reinterpret_cast<uint8_t *>(dtAlloc(tile.data.size(), DT_ALLOC_PERM));
            if (buffer == nullptr) {
                continue;
            }
            std::memcpy(buffer, tile.data.data(), tile.data.size());

            if (dtStatusFailed(tileCache->addTile(buffer, static_cast<int>(tile.data.size()), DT_COMPRESSEDTILE_FREE_DATA, nullptr))) {
                dtFree(buffer);
                continue;
            }
            tileCache->buildNavMeshTilesAt(tile.tx, tile.ty, navMesh);
        }

        BuildNavQuery();
        return true;
    }

    NaviQueryResult RecastNaviMesh::FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr& filter, const NaviPathQueryParam &param) const
    {
        if (navQuery == nullptr || navMesh == nullptr || filter == nullptr) {
            return NaviQueryResult::FAILED;
        }

        auto *rcFilter = static_cast<RecastQueryFilter *>(filter.Get());
        if (rcFilter == nullptr || rcFilter->GetFilter() == nullptr) {
            return NaviQueryResult::FAILED;
        }

        static const float ext[] = {5.f, 5.f, 5.f};

        dtPolyRef startPoly = 0;
        dtPolyRef endPoly   = 0;

        Vector3 rcStart = {};
        Vector3 rcEnd   = {};

        if (dtStatusFailed(navQuery->findNearestPoly(start.v, ext, rcFilter->GetFilter(), &startPoly, rcStart.v)) || startPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        if (dtStatusFailed(navQuery->findNearestPoly(end.v, ext, rcFilter->GetFilter(), &endPoly, rcEnd.v)) || endPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        std::vector<dtPolyRef> queryPath(RECAST_MAX_QUERY_PATH);
        int pathCount = 0;
        if (dtStatusFailed(navQuery->findPath(startPoly, endPoly, rcStart.v, rcEnd.v, rcFilter->GetFilter(),
                                              queryPath.data(), &pathCount, RECAST_MAX_QUERY_PATH)) || pathCount == 0) {
            return NaviQueryResult::FAILED;
        }
        queryPath.resize(pathCount);

        std::vector<Vector3>   straightPath(RECAST_MAX_QUERY_PATH * 3);
        std::vector<uint8_t>   straightPathFlags(RECAST_MAX_QUERY_PATH);
        std::vector<dtPolyRef> straightPathPolys(RECAST_MAX_QUERY_PATH);
        int straightCount = 0;
        if (dtStatusFailed(navQuery->findStraightPath(start.v, end.v, queryPath.data(), pathCount,
                                                      reinterpret_cast<float *>(straightPath.data()),
                                                      straightPathFlags.data(), straightPathPolys.data(), &straightCount,
                                                      RECAST_MAX_QUERY_PATH)) || straightCount == 0) {
            return NaviQueryResult::FAILED;
        }

        return NaviQueryResult::SUCCESS;
    }

} // namespace sky::ai
