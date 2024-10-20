//
// Created by blues on 2024/9/6.
//

#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastConversion.h>
#include <recast/RecastTileGenerator.h>
#include <navigation/NavigationSystem.h>
#include <core/math/MathUtil.h>

#include <physics/components/CollisionComponent.h>

namespace sky::ai {

    void RecastNaviMeshGenerator::Setup(const WorldPtr &inWorld)
    {
        world = inWorld;

        auto *navSys = static_cast<NavigationSystem*>(world->GetSubSystem(NavigationSystem::NAME.data()));
        navMesh = static_cast<RecastNaviMesh*>(navSys->GetNaviMesh().Get());

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

    bool RecastNaviMeshGenerator::DoWork()
    {
        const auto &bound = navMesh->GetBounds();
        const auto &ext = bound.max - bound.min;
        auto maxExt = std::max(std::max(ext.x, ext.y), ext.z);

        std::unique_ptr<NaviOctree> octree = std::make_unique<NaviOctree>(maxExt);
        GatherGeometry(octree.get());

        std::vector<RecastTile> pendingTiles;
        std::vector<CounterPtr<RecastTileGenerator>> tileGenerators;

        for (auto &tileCoord : pendingTiles) {
            RecastTileBuildParam param = {};
            param.coord = tileCoord;

            CounterPtr<RecastTileGenerator> generator = new RecastTileGenerator(config, param);
            generator->StartAsync();

            tileGenerators.emplace_back(generator);
        }

        return true;
    }

    void RecastNaviMeshGenerator::OnComplete(bool result)
    {

    }

} // namespace sky::ai