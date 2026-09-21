//
// Created by blues on 2024/9/6.
//

#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastConversion.h>
#include <recast/RecastConstants.h>
#include <recast/RecastLz4Compressor.h>
#include <recast/RecastTileCacheMeshProcessor.h>
#include <recast/RecastQueryFilter.h>

#include <navigation/NavigationSystem.h>
#include <core/math/MathUtil.h>

#include <physics/components/CollisionComponent.h>
#include <framework/world/TransformComponent.h>

#include <DetourNavMesh.h>

namespace sky::ai {
    static RecastTileCacheMeshProcessor GMeshProcessor;
    static dtTileCacheAlloc GAllocator;

    RecastNaviMeshGenerator::~RecastNaviMeshGenerator()
    {
        if (tileCache != nullptr) {
            dtFreeTileCache(tileCache);
            tileCache = nullptr;
        }
    }

    void RecastNaviMeshGenerator::Setup(const WorldPtr &inWorld)
    {
        world = inWorld;

        auto *navSys = static_cast<NavigationSystem*>(world->GetSubSystem(Name(NavigationSystem::NAME.data())));
        navMesh = static_cast<RecastNaviMesh*>(navSys->GetNaviMesh().Get());
        navMesh->SetBounds({{-50.f, -50.f, -50.f}, {50.f, 50.f, 50.f}});
        navMesh->PrepareForBuild();

        const auto &resolution = navMesh->GetResolution();
        const auto &agentCfg = navMesh->GetAgentConfig();

        config.cs = resolution.cellSize;
        config.ch = resolution.cellHeight;

        config.walkableSlopeAngle = agentCfg.maxSlope;
        config.walkableHeight     = CeilTo<int>(agentCfg.height / config.ch);
        config.walkableClimb      = CeilTo<int>(agentCfg.maxClimb / config.ch);
        config.walkableRadius     = CeilTo<int>(agentCfg.radius / config.cs);

        config.tileSize   = FloorTo<int>(resolution.tileSize / config.cs);
        config.borderSize = config.walkableRadius + 3;
        config.width      = config.tileSize + config.borderSize * 2;
        config.height     = config.tileSize + config.borderSize * 2;

        // built-in params.s
        config.maxEdgeLen = static_cast<int>(12.f / config.cs);
        config.maxSimplificationError = 1.3f;
        config.minRegionArea          = static_cast<int>(8.f);
        config.mergeRegionArea        = static_cast<int>(20.f);
        config.maxVertsPerPoly        = 6;
        config.detailSampleDist       = 6.f;
        config.detailSampleMaxError   = 1.f;

        const auto &bound = navMesh->GetBounds();
        ToRecast(bound.min, config.bmin);
        ToRecast(bound.max, config.bmax);
    }

    void RecastNaviMeshGenerator::GatherGeometry(NaviOctree* octree)
    {
        const auto &actors = world->GetActors();
        for (const auto &actor : actors) {
            auto *comp = actor->GetComponent<phy::CollisionComponent>();
            if (comp == nullptr) {
                continue;
            }
            auto *trans = actor->GetComponent<TransformComponent>();
            SKY_ASSERT(trans != nullptr);
            const auto &worldMatrix = trans->GetWorldMatrix();

            auto *shape = comp->GetPhysicsShape();
            auto triangleMesh = shape->GetTriangleMesh();
            if (!triangleMesh) {
                continue;
            }

            auto *scaledTriangleMesh = new TriangleMesh();
            scaledTriangleMesh->position.resize(triangleMesh->position.size());
            scaledTriangleMesh->indexRaw = triangleMesh->indexRaw;
            scaledTriangleMesh->vtxStride = triangleMesh->vtxStride;
            scaledTriangleMesh->indexType = triangleMesh->indexType;
            scaledTriangleMesh->views = triangleMesh->views;

            auto vtxCount = triangleMesh->position.size() / sizeof(Vector3);
            const auto* src = reinterpret_cast<const Vector3*>(triangleMesh->position.data());
            auto* dst = reinterpret_cast<Vector3*>(scaledTriangleMesh->position.data());
            for (auto i = 0; i < vtxCount; ++i) {
                dst[i] = ToVec3(worldMatrix * Vector4(src[i].x, src[i].y, src[i].z, 1.0f));
            }

            for (uint32_t i = 0; i < triangleMesh->views.size(); ++i) {
                scaledTriangleMesh->views[i].aabb = AABB::Transform(triangleMesh->views[i].aabb, worldMatrix);

                auto *element = new NaviOctreeElement();
                element->triangleMesh = scaledTriangleMesh;
                element->viewIndex = i;

                octree->AddElement(element);
            }
        }
    }

    void RecastNaviMeshGenerator::PrepareTiles(std::vector<RecastTile> &tiles) const
    {
        const auto &min = config.bmin;
        const auto &max = config.bmax;

        int gw = 0;
        int gh = 0;

        rcCalcGridSize(min, max, config.cs, &gw, &gh);

        const int ts = config.tileSize;
        const int tw = (gw + ts - 1) / ts;
        const int th = (gh + ts - 1) / ts;

        const int sx = static_cast<int>(std::floor(min[0] / (static_cast<float>(config.tileSize) * config.cs)));
        const int sy = static_cast<int>(std::floor(min[2] / (static_cast<float>(config.tileSize) * config.cs)));

        for (int i = 0; i < tw; ++i) {
            for (int j = 0; j < th; ++j) {
                tiles.emplace_back(RecastTile{sx + i, sy + j});
            }
        }
    }

    void RecastNaviMeshGenerator::PrepareWork()
    {
        GatherGeometry(navMesh->GetOctree());
        PrepareTiles(pendingTiles);

        for (auto &tileCoord : pendingTiles) {
            RecastTileBuildParam param = {};
            param.coord = tileCoord;

            CounterPtr<RecastTileGenerator> generator = new RecastTileGenerator(config, param);
            generator->Setup(navMesh);
            generator->StartAsync();

            dependencies.emplace_back(generator->GetTask());
            tileGenerators.emplace_back(generator);
        }
    }

    bool RecastNaviMeshGenerator::BuildNavMesh()
    {
        RecastNaviMapConfig navConfig = {};
        if (!navMesh->BuildNavMesh(navConfig)) {
            return false;
        }

        for (auto &gen : tileGenerators) {
            const auto &param = gen->GetParam();
            tileCache->buildNavMeshTilesAt(param.coord.x,param.coord.y, navMesh->GetNavMesh());
        }

        navMesh->BuildNavQuery();
        return true;
    }

    bool RecastNaviMeshGenerator::PrepareTileCache()
    {
        tileCache = dtAllocTileCache();
        if (tileCache == nullptr) {
            return false;
        }

        const auto &agentConfig = navMesh->GetAgentConfig();

        dtTileCacheParams tcParams = {};
        tcParams.cs = config.cs;
        tcParams.ch = config.ch;
        tcParams.width = config.tileSize;
        tcParams.height = config.tileSize;
        tcParams.maxSimplificationError = config.maxSimplificationError;

        tcParams.walkableHeight = agentConfig.height;
        tcParams.walkableRadius = agentConfig.radius;
        tcParams.walkableClimb = agentConfig.maxClimb;

        tcParams.maxTiles = RECAST_MAX_BUILD_TILES;
        tcParams.maxObstacles = RECAST_MAX_OBSTACLES;

        auto status = tileCache->init(&tcParams, &GAllocator, GetOrCreateCompressor(), &GMeshProcessor);
        if (dtStatusFailed(status)) {
            dtFreeTileCache(tileCache);
            tileCache = nullptr;
            return false;
        }

        for (auto &generator : tileGenerators) {
            auto &tileData = generator->GetData();
            for (auto &tile : tileData) {
                if (tile.navData == nullptr || tile.navData->size == 0) {
                    continue;
                }

                status = tileCache->addTile(tile.navData->data, static_cast<int32_t>(tile.navData->size), DT_COMPRESSEDTILE_FREE_DATA, nullptr);
                if (dtStatusFailed(status)) {
                    // Failure leaves ownership with us: free once and detach from the owner object.
                    dtFree(tile.navData->data);
                    tile.navData->data = nullptr;
                } else {
                    // Success transfers ownership to the tile cache; detach so we do not double free.
                    tile.navData->data = nullptr;
                }
                tile.navData = nullptr;
            }
        }

        return true;
    }

    NaviMeshBuildParams RecastNaviMeshGenerator::GetBuildParams() const
    {
        NaviMeshBuildParams params;
        params.agent                  = navMesh->GetAgentConfig();
        params.resolution.cellSize    = config.cs;
        params.resolution.cellHeight  = config.ch;
        params.resolution.tileSize    = static_cast<float>(config.tileSize) * config.cs;
        params.bounds                 = navMesh->GetBounds();
        params.maxSimplificationError = config.maxSimplificationError;
        params.borderSize             = config.borderSize;
        params.version                = 1;
        return params;
    }

    void RecastNaviMeshGenerator::CollectTiles(NaviMeshData &out) const
    {
        out.mode   = NaviMeshExportMode::Tiled;
        out.params = GetBuildParams();
        out.tiles.clear();

        for (const auto &generator : tileGenerators) {
            const auto &coord    = generator->GetParam().coord;
            const auto &tileData = generator->GetData();

            for (uint32_t layer = 0; layer < static_cast<uint32_t>(tileData.size()); ++layer) {
                const auto &tile = tileData[layer];
                if (tile.navData == nullptr || tile.navData->size == 0) {
                    continue;
                }

                NaviMeshTilePayload payload;
                payload.tx    = coord.x;
                payload.ty    = coord.y;
                payload.layer = layer;
                payload.data.assign(tile.navData->data, tile.navData->data + tile.navData->size);
                out.tiles.emplace_back(std::move(payload));
            }
        }
    }

    bool RecastNaviMeshGenerator::DoWork()
    {
        if (!PrepareTileCache()) {
            return false;
        }

        if (!BuildNavMesh()) {
            return false;
        }
        return true;
    }
} // namespace sky::ai