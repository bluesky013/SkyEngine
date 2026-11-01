//
// Created on 2026/09/23.
//

#pragma once

#include <framework/world/World.h>

#include <navigation/NaviMeshTypes.h>
#include <navigation/terrain/TerrainGeometryProvider.h>
#include <navigation/terrain/TerrainNavRebuild.h>

#include <memory>

namespace sky::ai {

    class INavigationSystem;

    // Registers terrain as a nav mesh geometry source for a world and coordinates incremental
    // nav mesh rebuilds from terrain changes. Owns the provider + rebuild coordinator.
    class TerrainNavigationBridge {
    public:
        TerrainNavigationBridge() = default;
        ~TerrainNavigationBridge();

        void Setup(const WorldPtr &world, terrain::ITerrainSystem *terrain, const NaviMeshBuildParams &params);
        void Shutdown();

        TerrainNavRebuildCoordinator &GetRebuildCoordinator()
        {
            return coordinator;
        }

    private:
        INavigationSystem                       *navSystem = nullptr;
        std::unique_ptr<TerrainGeometryProvider> provider;
        TerrainNavRebuildCoordinator             coordinator;
    };

} // namespace sky::ai
