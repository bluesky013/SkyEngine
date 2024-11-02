//
// Created by blues on 2024/9/1.
//

#include <recast/RecastNaviMesh.h>
#include <navigation/NavigationSystem.h>

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/RenderSceneProxy.h>

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

    RecastNaviMesh::RecastNaviMesh()
    {
        primitive = std::make_unique<RenderPrimitive>();
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

    void RecastNaviMesh::SetTechnique(const RDGfxTechPtr &tech)
    {
        TechniqueInstance techInst = {tech};
        techInst.topo = rhi::PrimitiveTopology::TRIANGLE_LIST;
        primitive->techniques.clear();
        primitive->techniques.emplace_back(techInst);
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
    }

    void RecastNaviMesh::OnAttachToWorld(World &world)
    {
        if (debugDraw) {
            auto *renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem("RenderScene"))->GetRenderScene();
            renderScene->AddPrimitive(primitive.get());
        }
    }

    void RecastNaviMesh::OnDetachFromWorld(World &world)
    {
        if (debugDraw != nullptr) {
            auto *renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem("RenderScene"))->GetRenderScene();
            renderScene->RemovePrimitive(primitive.get());
        }
    }
} // namespace sky::ai