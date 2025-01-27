//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>
#include <recast/RecastConstants.h>
#include <recast/RecastQueryFilter.h>
#include <navigation/NavigationSystem.h>

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/RenderSceneProxy.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>


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

    struct NaviPrimitive : public RenderPrimitive {
        void UpdateBatch() override {}
    };

    RecastNaviMesh::RecastNaviMesh()
    {
        primitive = std::make_unique<NaviPrimitive>();
        debugDraw = std::make_unique<DebugRenderer>();

        auto techAsset = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/debug.tech");
        techAsset->BlockUntilLoaded();
        auto debugTech = CreateTechniqueFromAsset(techAsset);
        SetTechnique(debugTech);
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

    void RecastNaviMesh::BuildNavQuery()
    {
        navQuery = dtAllocNavMeshQuery();
        auto status = navQuery->init(navMesh, RECAST_MAX_QUERY_NODES);
        if (dtStatusFailed(status)) {
            DebugDetourStatusDetail(status);
        }
    }

    void RecastNaviMesh::SetTechnique(const RDGfxTechPtr &tech)
    {
        RenderBatch batch = {tech};
        batch.topo = rhi::PrimitiveTopology::TRIANGLE_LIST;
        primitive->batches.clear();
        primitive->batches.emplace_back(batch);
    }

    void RecastNaviMesh::BuildDebugDraw()
    {
        debugDraw->Reset();
        RecastDrawNavMeshPolys(*navMesh, *debugDraw);

        debugDraw->Render(primitive.get());
    }

    void RecastNaviMesh::ResetNavMesh()
    {
        if (navMesh != nullptr) {
            dtFreeNavMesh(navMesh);
        }

        if (navQuery != nullptr) {
            dtFreeNavMeshQuery(navQuery);
        }
    }

    void RecastNaviMesh::OnAttachToWorld(World &world)
    {
        if (debugDraw) {
            auto *renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem(Name("RenderScene")))->GetRenderScene();
            renderScene->AddPrimitive(primitive.get());
        }
    }

    void RecastNaviMesh::OnDetachFromWorld(World &world)
    {
        if (debugDraw != nullptr) {
            auto *renderScene = static_cast<RenderSceneProxy *>(world.GetSubSystem(Name("RenderScene")))->GetRenderScene();
            renderScene->RemovePrimitive(primitive.get());
        }
    }

    NaviQueryResult RecastNaviMesh::FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr& filter, const NaviPathQueryParam &param) const
    {
        auto *rcFilter = static_cast<RecastQueryFilter*>(filter.Get());

        dtPolyRef startPoly = 0;
        dtPolyRef endPoly = 0;

        Vector3 rcStart = {};
        Vector3 rcEnd = {};

        static const float ext[] = {5.f, 5.f, 5.f};
        navQuery->findNearestPoly(start.v, ext, rcFilter->GetFilter(), &startPoly, rcStart.v);
        if (startPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        navQuery->findNearestPoly(end.v, ext, rcFilter->GetFilter(), &endPoly, rcEnd.v);
        if (startPoly == 0) {
            return NaviQueryResult::FAILED;
        }

        std::vector<dtPolyRef> queryPath(RECAST_MAX_QUERY_PATH);
        int pathCount = 0;
        navQuery->findPath(startPoly, endPoly, rcStart.v, rcEnd.v, rcFilter->GetFilter(), queryPath.data(), &pathCount, RECAST_MAX_QUERY_PATH);
        if (pathCount == 0) {
            return NaviQueryResult::FAILED;
        }
        queryPath.resize(pathCount);

        std::vector<Vector3>   straightPath(RECAST_MAX_QUERY_PATH * 3);
        std::vector<uint8_t>   straightPathFlags(RECAST_MAX_QUERY_PATH);
        std::vector<dtPolyRef> straightPathPolys(RECAST_MAX_QUERY_PATH);
        navQuery->findStraightPath(start.v, end.v, queryPath.data(), pathCount, reinterpret_cast<float *>(straightPath.data()),
                                   straightPathFlags.data(), straightPathPolys.data(), &pathCount, RECAST_MAX_QUERY_PATH);


        return NaviQueryResult::SUCCESS;
    }
} // namespace sky::ai