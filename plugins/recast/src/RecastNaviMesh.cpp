//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>

#include <recast/RecastConstants.h>
#include <recast/RecastDebugDraw.h>
#include <recast/RecastLz4Compressor.h>
#include <recast/RecastQueryFilter.h>
#include <recast/RecastTileCacheMeshProcessor.h>

#include <navigation/NaviMeshTypes.h>
#include <navigation/NaviPath.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourTileCache.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>

namespace sky::ai {

    RecastNavData::~RecastNavData()
    {
        if (data != nullptr) {
            dtFree(data);
        }
    }

    RecastNaviMesh::RecastNaviMesh() = default;

    RecastNaviMesh::~RecastNaviMesh()
    {
        ResetNavMesh();
    }

    bool RecastNaviMesh::BuildNavMesh(const RecastNaviMapConfig &config)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        ResetNavMesh();
        return BuildNavMeshUnlocked(config);
    }

    bool RecastNaviMesh::BuildNavMeshUnlocked(const RecastNaviMapConfig &config)
    {
        navMesh = dtAllocNavMesh();
        if (navMesh == nullptr) {
            return false;
        }

        dtNavMeshParams params = {};
        std::copy(buildBounds.min.v, buildBounds.min.v + 3, params.orig);
        params.tileWidth  = resolution.tileSize;
        params.tileHeight = resolution.tileSize;
        params.maxTiles   = static_cast<int>(config.maxTiles);
        params.maxPolys   = static_cast<int>(config.maxPolys);
#ifndef DT_POLYREF64
        if (config.maxTiles == 0 || config.maxTiles > (1U << 20)) {
            ResetNavMesh();
            return false;
        }
        uint32_t tileBits = 0;
        for (uint32_t capacity = 1; capacity < config.maxTiles; capacity <<= 1) {
            ++tileBits;
        }
        // Detour reserves at least ten salt bits in a 32-bit polygon reference.
        params.maxPolys = static_cast<int>(std::min(config.maxPolys, 1U << (22U - tileBits)));
#endif

        auto status = navMesh->init(&params);
        if (dtStatusFailed(status)) {
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
            return false;
        }

        return true;
    }

    bool RecastNaviMesh::BuildNavQuery()
    {
        std::shared_lock<std::shared_mutex> lock(queryMutex);
        return BuildNavQueryUnlocked();
    }

    bool RecastNaviMesh::BuildNavQueryUnlocked()
    {
        if (navMesh == nullptr) {
            return false;
        }

        dtNavMeshQuery query;
        return !dtStatusFailed(query.init(navMesh, RECAST_MAX_QUERY_NODES));
    }

    void RecastNaviMesh::BuildDebugGeometry(NaviDebugGeometry &out) const
    {
        std::shared_lock<std::shared_mutex> lock(queryMutex);
        if (navMesh == nullptr) {
            out.vertices.clear();
            return;
        }

        RecastBuildNavMeshGeometry(*navMesh, out);
    }

    namespace {
        struct NavMeshSetHeader {
            int32_t         version;
            int32_t         tileCount;
            dtNavMeshParams params;
        };

        struct NavMeshTileHeader {
            dtTileRef tileRef;
            int32_t   dataSize;
        };

        constexpr int32_t NAVMESH_SET_VERSION = 1;
    } // namespace

    bool RecastNaviMesh::Serialize(std::vector<uint8_t> &out) const
    {
        std::shared_lock<std::shared_mutex> lock(queryMutex);
        return SerializeUnlocked(out);
    }

    bool RecastNaviMesh::SerializeUnlocked(std::vector<uint8_t> &out) const
    {
        if (navMesh == nullptr) {
            return false;
        }

        const dtNavMeshParams *params = navMesh->getParams();
        if (params == nullptr) {
            return false;
        }

        const dtNavMesh *mesh = navMesh;

        NavMeshSetHeader header = {};
        header.version          = NAVMESH_SET_VERSION;
        header.tileCount        = 0;
        header.params           = *params;

        for (int i = 0; i < mesh->getMaxTiles(); ++i) {
            const dtMeshTile *tile = mesh->getTile(i);
            if (tile != nullptr && tile->header != nullptr) {
                header.tileCount++;
            }
        }

        out.clear();
        const auto append = [&out](const void *data, size_t size) {
            const auto *bytes = static_cast<const uint8_t *>(data);
            out.insert(out.end(), bytes, bytes + size);
        };

        append(&header, sizeof(header));

        for (int i = 0; i < mesh->getMaxTiles(); ++i) {
            const dtMeshTile *tile = mesh->getTile(i);
            if (tile == nullptr || tile->header == nullptr) {
                continue;
            }

            NavMeshTileHeader tileHeader = {};
            tileHeader.tileRef           = mesh->getTileRef(tile);
            tileHeader.dataSize          = static_cast<int32_t>(tile->dataSize);
            append(&tileHeader, sizeof(tileHeader));
            append(tile->data, tile->dataSize);
        }

        return true;
    }

    bool RecastNaviMesh::Deserialize(const std::vector<uint8_t> &in)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        return DeserializeUnlocked(in);
    }

    bool RecastNaviMesh::DeserializeUnlocked(const std::vector<uint8_t> &in)
    {
        ResetNavMesh();

        if (in.size() < sizeof(NavMeshSetHeader)) {
            return false;
        }

        size_t           offset = 0;
        NavMeshSetHeader header;
        std::memcpy(&header, in.data(), sizeof(header));
        offset += sizeof(header);

        if (header.version != NAVMESH_SET_VERSION || header.tileCount < 0 || header.params.maxTiles <= 0 || header.params.maxTiles > (1 << 20) ||
            header.params.maxPolys <= 0 || header.params.maxPolys > (1 << 22) || header.tileCount > header.params.maxTiles ||
            static_cast<size_t>(header.tileCount) > (in.size() - offset) / (sizeof(NavMeshTileHeader) + sizeof(dtMeshHeader)) ||
            !std::isfinite(header.params.tileWidth) || header.params.tileWidth <= 0.f || !std::isfinite(header.params.tileHeight) ||
            header.params.tileHeight <= 0.f) {
            return false;
        }

        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(header.params.orig[i])) {
                return false;
            }
        }
#ifndef DT_POLYREF64
        uint32_t tileBits = 0, polyBits = 0;
        for (uint32_t capacity = 1; capacity < static_cast<uint32_t>(header.params.maxTiles); capacity <<= 1)
            ++tileBits;
        for (uint32_t capacity = 1; capacity < static_cast<uint32_t>(header.params.maxPolys); capacity <<= 1)
            ++polyBits;
        if (tileBits + polyBits > 22) {
            return false;
        }
#endif
        navMesh = dtAllocNavMesh();
        if (navMesh == nullptr) {
            return false;
        }

        if (dtStatusFailed(navMesh->init(&header.params))) {
            ResetNavMesh();
            return false;
        }

        for (int32_t i = 0; i < header.tileCount; ++i) {
            if (offset + sizeof(NavMeshTileHeader) > in.size()) {
                ResetNavMesh();
                return false;
            }

            NavMeshTileHeader tileHeader;
            std::memcpy(&tileHeader, in.data() + offset, sizeof(tileHeader));
            offset += sizeof(tileHeader);

            if (tileHeader.dataSize < static_cast<int32_t>(sizeof(dtMeshHeader)) || static_cast<size_t>(tileHeader.dataSize) > in.size() - offset) {
                ResetNavMesh();
                return false;
            }

            dtMeshHeader tile = {};
            std::memcpy(&tile, in.data() + offset, sizeof(tile));
            size_t     required = (sizeof(tile) + 3u) & ~size_t(3);
            const auto addBlock = [&](int count, size_t stride) {
                if (count < 0 || static_cast<size_t>(count) > static_cast<size_t>(tileHeader.dataSize) / stride) {
                    return false;
                }
                const size_t size = (static_cast<size_t>(count) * stride + 3u) & ~size_t(3);
                if (required > static_cast<size_t>(tileHeader.dataSize) || size > static_cast<size_t>(tileHeader.dataSize) - required) {
                    return false;
                }
                required += size;
                return true;
            };
            if (tile.magic != DT_NAVMESH_MAGIC || tile.version != DT_NAVMESH_VERSION || tile.polyCount <= 0 ||
                tile.polyCount > header.params.maxPolys || tile.vertCount <= 0 || tile.vertCount > 65535 || tile.detailMeshCount < 0 ||
                tile.detailMeshCount > tile.polyCount || tile.offMeshBase < 0 || tile.offMeshBase > tile.polyCount ||
                !addBlock(tile.vertCount, sizeof(float) * 3) || !addBlock(tile.polyCount, sizeof(dtPoly)) ||
                !addBlock(tile.maxLinkCount, sizeof(dtLink)) || !addBlock(tile.detailMeshCount, sizeof(dtPolyDetail)) ||
                !addBlock(tile.detailVertCount, sizeof(float) * 3) || !addBlock(tile.detailTriCount, 4) ||
                !addBlock(tile.bvNodeCount, sizeof(dtBVNode)) || !addBlock(tile.offMeshConCount, sizeof(dtOffMeshConnection))) {
                ResetNavMesh();
                return false;
            }

            auto *data = static_cast<uint8_t *>(dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM));
            if (data == nullptr) {
                ResetNavMesh();
                return false;
            }
            std::memcpy(data, in.data() + offset, tileHeader.dataSize);
            offset += tileHeader.dataSize;

            if (dtStatusFailed(navMesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0))) {
                dtFree(data);
                ResetNavMesh();
                return false;
            }
        }

        if (!BuildNavQueryUnlocked()) {
            ResetNavMesh();
            return false;
        }
        return true;
    }

    void RecastNaviMesh::ResetNavMesh()
    {
        if (tileCache != nullptr) {
            dtFreeTileCache(tileCache);
            tileCache = nullptr;
        }

        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
        }
    }

    bool RecastNaviMesh::InitTileCache(const NaviMeshBuildParams &params)
    {
        tileCache = dtAllocTileCache();
        if (tileCache == nullptr) {
            return false;
        }

        dtTileCacheParams tcParams = {};
        std::copy(buildBounds.min.v, buildBounds.min.v + 3, tcParams.orig);
        tcParams.cs                     = resolution.cellSize;
        tcParams.ch                     = resolution.cellHeight;
        tcParams.width                  = resolution.cellSize > 0.f ? static_cast<int>(resolution.tileSize / resolution.cellSize) : 0;
        tcParams.height                 = tcParams.width;
        tcParams.maxSimplificationError = params.maxSimplificationError;
        tcParams.walkableHeight         = agentCfg.height;
        tcParams.walkableRadius         = agentCfg.radius;
        tcParams.walkableClimb          = agentCfg.maxClimb;
        tcParams.maxTiles               = static_cast<int>(GetMaxTiles());
        tcParams.maxObstacles           = RECAST_MAX_OBSTACLES;

        if (dtStatusFailed(tileCache->init(&tcParams, &tileCacheAlloc, &compressor, &meshProcessor))) {
            dtFreeTileCache(tileCache);
            tileCache = nullptr;
            return false;
        }

        return true;
    }

    bool RecastNaviMesh::PrepareStreaming(const NaviMeshBuildParams &params)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        return PrepareStreamingUnlocked(params);
    }

    bool RecastNaviMesh::PrepareStreamingUnlocked(const NaviMeshBuildParams &params)
    {
        ResetNavMesh();

        const auto &res   = params.resolution;
        const auto &agent = params.agent;
        if (params.version != 1 || !std::isfinite(res.cellSize) || res.cellSize <= 0.f || !std::isfinite(res.cellHeight) || res.cellHeight <= 0.f ||
            !std::isfinite(res.tileSize) || res.tileSize < res.cellSize || res.tileSize / res.cellSize > 255.f || !std::isfinite(agent.height) ||
            agent.height <= 0.f || !std::isfinite(agent.radius) || agent.radius < 0.f || !std::isfinite(agent.maxClimb) || agent.maxClimb < 0.f ||
            !std::isfinite(agent.maxSlope) || agent.maxSlope < 0.f || agent.maxSlope >= 90.f || !std::isfinite(params.maxSimplificationError) ||
            params.maxSimplificationError < 0.f) {
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(params.bounds.min.v[i]) || !std::isfinite(params.bounds.max.v[i]) ||
                params.bounds.min.v[i] >= params.bounds.max.v[i]) {
                return false;
            }
        }

        agentCfg    = params.agent;
        resolution  = params.resolution;
        buildBounds = params.bounds;
        SetMaxSimplificationError(params.maxSimplificationError);

        RecastNaviMapConfig navConfig = {};
        navConfig.maxTiles            = GetMaxTiles();
        if (!BuildNavMeshUnlocked(navConfig)) {
            return false;
        }

        if (!InitTileCache(params)) {
            ResetNavMesh();
            return false;
        }

        if (!BuildNavQueryUnlocked()) {
            ResetNavMesh();
            return false;
        }
        return true;
    }

    bool RecastNaviMesh::AddTile(const NaviMeshTilePayload &tile)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        return AddTileUnlocked(tile);
    }

    bool RecastNaviMesh::AddTileUnlocked(const NaviMeshTilePayload &tile)
    {
        if (navMesh == nullptr || tileCache == nullptr || tile.data.size() < sizeof(dtTileCacheLayerHeader) ||
            tile.data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return false;
        }

        dtTileCacheLayerHeader header = {};
        std::memcpy(&header, tile.data.data(), sizeof(header));
        if (header.magic != DT_TILECACHE_MAGIC || header.version != DT_TILECACHE_VERSION || header.tx != tile.tx || header.ty != tile.ty ||
            header.tlayer != static_cast<int>(tile.layer) || header.width == 0 || header.height == 0 ||
            header.width > tileCache->getParams()->width || header.height > tileCache->getParams()->height || header.minx > header.maxx ||
            header.miny > header.maxy || header.maxx >= header.width || header.maxy >= header.height ||
            tileCache->getTileAt(tile.tx, tile.ty, header.tlayer) != nullptr) {
            return false;
        }

        auto *buffer = static_cast<uint8_t *>(dtAlloc(tile.data.size(), DT_ALLOC_PERM));
        if (buffer == nullptr) {
            return false;
        }
        std::memcpy(buffer, tile.data.data(), tile.data.size());

        dtCompressedTileRef tileRef = 0;
        if (dtStatusFailed(tileCache->addTile(buffer, static_cast<int>(tile.data.size()), DT_COMPRESSEDTILE_FREE_DATA, &tileRef))) {
            dtFree(buffer);
            return false;
        }

        if (dtStatusFailed(tileCache->buildNavMeshTile(tileRef, navMesh))) {
            tileCache->removeTile(tileRef, nullptr, nullptr);
            return false;
        }
        return true;
    }

    bool RecastNaviMesh::LoadData(const NaviMeshData &data)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        if (data.mode == NaviMeshExportMode::Full) {
            if (!DeserializeUnlocked(data.fullData)) {
                return false;
            }
            agentCfg    = data.params.agent;
            resolution  = data.params.resolution;
            buildBounds = data.params.bounds;
            SetMaxSimplificationError(data.params.maxSimplificationError);
            return true;
        }

        if (data.mode != NaviMeshExportMode::Tiled) {
            return false;
        }

        if (!PrepareStreamingUnlocked(data.params)) {
            return false;
        }

        for (const auto &tile : data.tiles) {
            if (!AddTileUnlocked(tile)) {
                ResetNavMesh();
                return false;
            }
        }

        return true;
    }

    bool RecastNaviMesh::RemoveTile(const NaviMeshTileCoord &coord)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        return RemoveTileUnlocked(coord);
    }

    bool RecastNaviMesh::RemoveTileUnlocked(const NaviMeshTileCoord &coord)
    {
        if (navMesh == nullptr) {
            return false;
        }

        if (tileCache != nullptr) {
            for (int i = 0; i < tileCache->getTileCount(); ++i) {
                const auto *tile = tileCache->getTile(i);
                if (tile->header != nullptr && tile->header->tx == coord.x && tile->header->ty == coord.y) {
                    if (dtStatusFailed(tileCache->removeTile(tileCache->getTileRef(tile), nullptr, nullptr))) {
                        return false;
                    }
                }
            }
        }

        if (navMesh != nullptr) {
            for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
                const auto *tile = static_cast<const dtNavMesh *>(navMesh)->getTile(i);
                if (tile->header != nullptr && tile->header->x == coord.x && tile->header->y == coord.y) {
                    if (dtStatusFailed(navMesh->removeTile(navMesh->getTileRef(tile), nullptr, nullptr))) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool RecastNaviMesh::ApplyBuild(const NaviMeshData &data, const std::vector<NaviMeshTileCoord> &rebuiltTiles)
    {
        std::unique_lock<std::shared_mutex> lock(queryMutex);
        if (data.mode != NaviMeshExportMode::Tiled) {
            return false;
        }
        if (rebuiltTiles.empty()) {
            if (!PrepareStreamingUnlocked(data.params)) {
                return false;
            }
        } else {
            if (navMesh == nullptr || tileCache == nullptr) {
                return false;
            }
            const auto *params = tileCache->getParams();
            if (params->cs != data.params.resolution.cellSize || params->ch != data.params.resolution.cellHeight ||
                params->width * params->cs != data.params.resolution.tileSize || params->walkableHeight != data.params.agent.height ||
                params->walkableRadius != data.params.agent.radius || params->walkableClimb != data.params.agent.maxClimb ||
                params->maxSimplificationError != data.params.maxSimplificationError) {
                return false;
            }
            for (int i = 0; i < 3; ++i) {
                if (params->orig[i] != data.params.bounds.min.v[i]) {
                    return false;
                }
            }
            for (const auto &coord : rebuiltTiles) {
                RemoveTileUnlocked(coord);
            }
        }
        for (const auto &tile : data.tiles) {
            if (!AddTileUnlocked(tile)) {
                return false;
            }
        }
        return true;
    }

    NaviQueryResult
    RecastNaviMesh::FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &param) const
    {
        NaviPath path;
        return QueryPath(start, end, filter, param, path);
    }

    NaviQueryResult RecastNaviMesh::QueryPath(
        const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &param, NaviPath &out) const
    {
        out.Reset();

        std::shared_lock<std::shared_mutex> lock(queryMutex);
        if (navMesh == nullptr || filter == nullptr) {
            return NaviQueryResult::FAILED;
        }

        dtNavMeshQuery query;
        if (dtStatusFailed(query.init(navMesh, RECAST_MAX_QUERY_NODES))) {
            return NaviQueryResult::FAILED;
        }

        dtQueryFilter rcFilter;
        rcFilter.setIncludeFlags(filter->GetIncludeFlags());
        rcFilter.setExcludeFlags(filter->GetExcludeFlags());
        for (AreaId i = 0; i < DT_MAX_AREAS; ++i) {
            const float cost = filter->GetAreaCost(i);
            if (!std::isfinite(cost) || cost < 1.f) {
                return NaviQueryResult::FAILED;
            }
            rcFilter.setAreaCost(i, cost);
        }
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(start.v[i]) || !std::isfinite(end.v[i])) {
                return NaviQueryResult::FAILED;
            }
        }

        static const float ext[] = {5.f, 5.f, 5.f};

        dtPolyRef startPoly = 0;
        dtPolyRef endPoly   = 0;

        Vector3 rcStart = {};
        Vector3 rcEnd   = {};

        if (dtStatusFailed(query.findNearestPoly(start.v, ext, &rcFilter, &startPoly, rcStart.v)) || startPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        if (dtStatusFailed(query.findNearestPoly(end.v, ext, &rcFilter, &endPoly, rcEnd.v)) || endPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        std::vector<dtPolyRef> queryPath(RECAST_MAX_QUERY_PATH);
        int                    pathCount = 0;
        const auto             pathStatus =
            query.findPath(startPoly, endPoly, rcStart.v, rcEnd.v, &rcFilter, queryPath.data(), &pathCount, RECAST_MAX_QUERY_PATH);
        if (dtStatusFailed(pathStatus) || dtStatusDetail(pathStatus, DT_PARTIAL_RESULT | DT_BUFFER_TOO_SMALL | DT_OUT_OF_NODES) || pathCount == 0 ||
            queryPath[pathCount - 1] != endPoly) {
            return NaviQueryResult::FAILED;
        }
        queryPath.resize(pathCount);

        std::vector<Vector3>   straightPath(RECAST_MAX_QUERY_PATH);
        std::vector<uint8_t>   straightPathFlags(RECAST_MAX_QUERY_PATH);
        std::vector<dtPolyRef> straightPathPolys(RECAST_MAX_QUERY_PATH);
        int                    straightCount  = 0;
        const auto             straightStatus = query.findStraightPath(rcStart.v, rcEnd.v, queryPath.data(), pathCount, straightPath.front().v,
                                                                       straightPathFlags.data(), straightPathPolys.data(), &straightCount, RECAST_MAX_QUERY_PATH);
        if (dtStatusFailed(straightStatus) || dtStatusDetail(straightStatus, DT_BUFFER_TOO_SMALL) || straightCount == 0) {
            return NaviQueryResult::FAILED;
        }

        out.points.reserve(static_cast<size_t>(straightCount));
        out.flags.reserve(static_cast<size_t>(straightCount));
        for (int i = 0; i < straightCount; ++i) {
            out.points.push_back(straightPath[i]);
            out.flags.push_back(straightPathFlags[i]);
        }
        out.result = NaviQueryResult::SUCCESS;

        return NaviQueryResult::SUCCESS;
    }

} // namespace sky::ai
