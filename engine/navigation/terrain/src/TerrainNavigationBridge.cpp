//
// Created on 2026/09/23.
//

#include <navigation/terrain/TerrainNavigationBridge.h>

#include <navigation/NavigationSystem.h>

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

        navSystem = static_cast<NavigationSystem *>(world->GetSubSystem(Name(NavigationSystem::NAME.data())));
        if (navSystem == nullptr) {
            return;
        }

        provider = std::make_unique<TerrainGeometryProvider>(terrain);
        navSystem->AddGeometryProvider(provider.get());

        coordinator.Setup(terrain, params, world);
    }

    void TerrainNavigationBridge::Shutdown()
    {
        coordinator.Shutdown();
        if (navSystem != nullptr && provider != nullptr) {
            navSystem->RemoveGeometryProvider(provider.get());
        }
        provider.reset();
        navSystem = nullptr;
    }

} // namespace sky::ai
