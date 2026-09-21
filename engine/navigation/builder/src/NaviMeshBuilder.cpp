//
// Created on 2026/09/21.
//

#include <builder/navigation/NaviMeshBuilder.h>

#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NaviMeshGenerator.h>
#include <navigation/NaviMeshSource.h>
#include <navigation/NavigationSystem.h>

#include <core/async/Task.h>
#include <core/file/FileSystem.h>
#include <core/name/Name.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/World.h>

namespace sky::builder {

    std::string_view NaviMeshBuilder::QueryType(const std::string &ext) const
    {
        return AssetTraits<ai::NaviMesh>::ASSET_TYPE;
    }

    void NaviMeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        ai::NaviMeshSourceData source;
        {
            auto archive = request.file->ReadAsArchive();
            if (!archive || !archive->IsOpen()) {
                result.retCode = AssetBuildRetCode::FAILED;
                return;
            }
            JsonInputArchive json(*archive);
            json.LoadValueObject(source);
        }

        if (source.scene.empty()) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        WorldPtr world = World::CreateWorld();
        {
            const auto scenePath = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath() / FilePath(source.scene);
            NativeFile sceneFile(scenePath);
            auto       sceneArchive = sceneFile.ReadAsArchive();
            if (!sceneArchive || !sceneArchive->IsOpen()) {
                result.retCode = AssetBuildRetCode::FAILED;
                return;
            }
            JsonInputArchive json(*sceneArchive);
            world->LoadJson(json);
        }

        world->AddSubSystem(Name(ai::NavigationSystem::NAME.data()), new ai::NavigationSystem());
        world->Init();

        auto *navSys = static_cast<ai::NavigationSystem *>(world->GetSubSystem(Name(ai::NavigationSystem::NAME.data())));
        if (navSys == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        auto navMesh = navSys->GetNaviMesh();
        if (navMesh == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        ai::NaviAgentConfig    agent;
        agent.height   = source.agentHeight;
        agent.radius   = source.agentRadius;
        agent.maxSlope = source.agentMaxSlope;
        agent.maxClimb = source.agentMaxClimb;

        ai::NaviMeshResolution resolution;
        resolution.cellSize   = source.cellSize;
        resolution.cellHeight = source.cellHeight;
        resolution.tileSize   = source.tileSize;

        AABB bounds;
        bounds.min = Vector3(source.boundsMinX, source.boundsMinY, source.boundsMinZ);
        bounds.max = Vector3(source.boundsMaxX, source.boundsMaxY, source.boundsMaxZ);

        navMesh->SetAgentConfig(agent);
        navMesh->SetResolution(resolution);
        navMesh->SetBounds(bounds);

        auto generator = ai::NaviMeshFactory::Get()->CreateGenerator();
        if (generator == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        generator->SetExportMode(source.exportMode == 0 ? ai::NaviMeshExportMode::Tiled : ai::NaviMeshExportMode::Full);
        generator->Setup(world);
        generator->StartAsync();
        TaskExecutor::Get()->WaitForAll();

        ai::NaviMeshData data;
        generator->CollectTiles(data);

        auto asset = AssetManager::Get()->FindOrCreateAsset<ai::NaviMesh>(request.assetInfo->uuid);
        asset->Data() = data;
        AssetManager::Get()->SaveAsset(asset, request.target);

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
