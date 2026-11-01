//
// Created on 2026/09/23.
//

#include <navigation/terrain/TerrainNavigationBridge.h>

#include <navigation/INavigationSystem.h>

namespace sky::ai {

    TerrainNavigationBridge::~TerrainNavigationBridge()
    {
        Shutdown();
    }

    void TerrainNavigationBridge::Setup(const WorldPtr &world, terrain::ITerrainSystem *terrain, const NaviMeshBuildParams &params)
    {
        Shutdown();
        if (world == nullptr || terrain == nullptr) {
            return;
        }

        navSystem = static_cast<INavigationSystem *>(world->GetSubSystem(Name(INavigationSystem::NAME.data())));
        if (navSystem == nullptr) {
            return;
        }

        provider = std::make_unique<TerrainGeometryProvider>(terrain);
        navSystem->AddGeometryProvider(provider.get());

        coordinator.Setup(terrain, params, world);
    }

    void TerrainNavigationBridge::Shutdown()
    {
        if (navSystem != nullptr && provider != nullptr) {
            navSystem->RemoveGeometryProvider(provider.get());
        }
        coordinator.Shutdown();
        provider.reset();
        navSystem = nullptr;
    }

} // namespace sky::ai
