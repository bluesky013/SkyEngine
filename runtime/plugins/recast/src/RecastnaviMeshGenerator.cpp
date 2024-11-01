//
// Created by blues on 2024/9/6.
//

#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastConversion.h>
#include <recast/RecastConstants.h>
#include <recast/RecastLz4Compressor.h>
#include <recast/RecastTileCacheMeshProcessor.h>
#include <recast/RecastDebugDraw.h>

#include <navigation/NavigationSystem.h>
#include <core/math/MathUtil.h>

#include <physics/components/CollisionComponent.h>

#include <DetourNavMesh.h>

namespace sky::ai {
    static RecastTileCacheMeshProcessor GMeshProcessor;
    static dtTileCacheAlloc GAllocator;

    void RecastNaviMeshGenerator::Setup(const WorldPtr &inWorld)
    {
        world = inWorld;

        auto *navSys = static_cast<NavigationSystem*>(world->GetSubSystem(NavigationSystem::NAME.data()));
        navMesh = static_cast<RecastNaviMesh*>(navSys->GetNaviMesh().Get());
        navMesh->SetBounds({{-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f}});
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

        // built-in params.
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

            auto *shape = comp->GetPhysicsShape();
            auto triangleMesh = shape->GetTriangleMesh();
            if (triangleMesh) {
                for (uint32_t i = 0; i < triangleMesh->views.size(); ++i) {
                    auto *element = new NaviOctreeElement();
                    element->triangleMesh = triangleMesh;
                    element->viewIndex = i;

                    octree->AddElement(element);
                }
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
        const int th = (gw + ts - 1) / ts;

        for (int i = 0; i < tw; ++i) {
            for (int j = 0; j < th; ++j) {
                tiles.emplace_back(RecastTile{i, j});
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

        return true;
    }

    bool RecastNaviMeshGenerator::PrepareTileCache()
    {
        tileCache = dtAllocTileCache();
        if (tileCache == nullptr) {
            return false;
        }

        const auto &agentConfig = navMesh->GetAgentConfig();

        dtTileCacheParams tcParams;
        memset(&tcParams, 0, sizeof(tcParams));
        rcVcopy(tcParams.orig, config.bmin);
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
            return false;
        }

        for (auto &generator : tileGenerators) {
            auto &tileData = generator->GetData();
            for (auto &tile : tileData) {
                status = tileCache->addTile(tile.navData->data, static_cast<int32_t>(tile.navData->size), DT_COMPRESSEDTILE_FREE_DATA, nullptr);
                if (dtStatusFailed(status))
                {
                    dtFree(tile.navData->data);
                    tile.navData = nullptr;
                    continue;
                }
            }
        }

        return true;
    }

    bool RecastNaviMeshGenerator::DoWork()
    {
        if (!PrepareTileCache()) {
            return false;
        }

        if (!BuildNavMesh()) {
            return false;
        }

        std::unique_ptr<RecastDebugDraw> debugRenderer = std::make_unique<RecastDebugDraw>();
        RecastDrawNavMeshPolys(*navMesh->GetNavMesh(), *debugRenderer);
        return true;
    }

    void RecastNaviMeshGenerator::OnComplete(bool result)
    {

    }

} // namespace sky::ai