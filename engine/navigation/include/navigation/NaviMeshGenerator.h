//
// Created by blues on 2024/10/11.
//

#pragma once

#include <core/async/Task.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshAsset.h>
#include <framework/world/World.h>

#include <vector>

namespace sky::ai {

    class NaviMeshGenerator : public Task {
    public:
        NaviMeshGenerator() = default;
        ~NaviMeshGenerator() override = default;

        virtual void Setup(const WorldPtr &world) = 0;

        // Build params and per-tile payloads are emitted so offline tooling can persist a nav mesh asset
        // without touching backend types.
        virtual NaviMeshBuildParams GetBuildParams() const = 0;
        virtual void CollectTiles(NaviMeshData &out) const = 0;

        void SetExportMode(NaviMeshExportMode mode) { exportMode = mode; }
        NaviMeshExportMode GetExportMode() const { return exportMode; }

        // Restricts the build to a subset of tiles (incremental rebuild); empty means "all tiles".
        void SetRebuildTiles(const std::vector<NaviMeshTileCoord> &tiles) { rebuildTiles = tiles; }
        const std::vector<NaviMeshTileCoord> &GetRebuildTiles() const { return rebuildTiles; }

    protected:
        NaviMeshExportMode               exportMode = NaviMeshExportMode::Tiled;
        std::vector<NaviMeshTileCoord>   rebuildTiles;
    };

} // namespace sky::ai
