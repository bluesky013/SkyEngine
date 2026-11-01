//
// Created on 2026/09/21.
//

#include <builder/navigation/NaviMeshBuilder.h>

#include <navigation/INavigationSystem.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NaviMeshGenerator.h>
#include <navigation/NaviMeshSource.h>
#include <recast/NavigationSubSystemConfig.h>

#include <core/async/Task.h>
#include <core/file/FileSystem.h>
#include <core/name/Name.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

namespace sky::builder {

    std::string_view NaviMeshBuilder::QueryType(const std::string &ext) const
    {
        return AssetTraits<ai::NaviMesh>::ASSET_TYPE;
    }

    void NaviMeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.assetInfo || !request.file) {
            result.error = "Navigation cook requires an identified source file";
            return;
        }
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

        if (source.scene.empty() || source.exportMode > 1) {
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
            if (!world->LoadJson(json)) {
                result.retCode = AssetBuildRetCode::FAILED;
                return;
            }
        }

        auto &registry = WorldSubSystemRegistry::Get();
        if (!registry.IsRegistered(Name(ai::INavigationSystem::NAME.data()))) {
            result.error = "Navigation runtime backend is unavailable";
            return;
        }
        auto      &desc = *world->GetMutableWorldDesc();
        const Name navigation(ai::INavigationSystem::NAME.data());
        auto       found = std::find_if(desc.subSystems.begin(), desc.subSystems.end(), [&](const auto &entry) { return entry.name == navigation; });
        if (found == desc.subSystems.end())
            desc.subSystems.push_back({navigation, {}, true});
        else {
            ai::NavigationSubSystemConfig config;
            if (found->config) {
                const auto *authored = found->config.GetAsConst<ai::NavigationSubSystemConfig>();
                if (!authored) {
                    result.error = "Unexpected navigation config type in the source world";
                    return;
                }
                config = *authored;
            }
            config.navMeshAsset = {};
            config.streaming    = false;
            found->enabled      = true;
            found->config       = Any(std::in_place_type<ai::NavigationSubSystemConfig>, config);
        }
        if (!world->Build(desc)) {
            result.retCode = AssetBuildRetCode::FAILED;
            result.error   = world->GetLastError();
            return;
        }
        world->Init();

        bool prepared = false;
        for (uint32_t stage = 0; stage < 64; ++stage) {
            bool pending = false;
            for (const auto &actor : world->GetActors()) {
                for (const auto &[type, component] : actor->GetComponents()) {
                    std::string error;
                    const auto  state = component->PollWorldPreparation(error);
                    if (state == ComponentPreparation::Failed) {
                        result.error = "Actor '" + actor->GetName() + "', component " + type.ToString() + ": " + error;
                        return;
                    }
                    pending |= state == ComponentPreparation::Preparing;
                }
            }
            if (!pending) {
                prepared = true;
                break;
            }
            // Cooking runs on the cook pool; waiting for that pool here would wait on this request itself.
            AssetExecutor::Get()->WaitForLoads();
        }
        if (!prepared) {
            result.error = "Navigation world exceeded 64 asynchronous attachment stages";
            return;
        }

        auto *navSys = static_cast<ai::INavigationSystem *>(world->GetSubSystem(Name(ai::INavigationSystem::NAME.data())));
        if (navSys == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        auto navMesh = navSys->GetNaviMesh();
        if (navMesh == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        ai::NaviAgentConfig agent;
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
        navMesh->SetMaxSimplificationError(source.maxSimplificationError);

        auto generator = ai::NaviMeshFactory::Get()->CreateGenerator();
        if (generator == nullptr) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        generator->SetExportMode(source.exportMode == 0 ? ai::NaviMeshExportMode::Tiled : ai::NaviMeshExportMode::Full);
        generator->Setup(world);
        generator->StartAsync();
        TaskExecutor::Get()->WaitForAll();
        if (!generator->HasSucceeded()) {
            result.error = std::string(generator->GetError());
            return;
        }

        ai::NaviMeshData data;
        generator->CollectTiles(data);

        auto      *manager = AssetManager::Get();
        const auto asset   = std::static_pointer_cast<Asset<ai::NaviMesh>>(
            manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<ai::NaviMesh>::ASSET_TYPE.data())));
        if (!asset) {
            result.error = "Navigation asset handler is unavailable";
            return;
        }
        asset->Data() = std::move(data);
        if (const auto saved = manager->SaveAsset(asset, request.bundle.empty() ? request.target : request.bundle); !saved) {
            result.error = saved.message;
            return;
        }

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
