//
// Created on 2026/09/21.
//

#pragma once

#include <navigation/NaviMesh.h>

#include <cstdint>
#include <vector>

namespace sky::ai {

    enum class NaviMeshExportMode : uint32_t {
        Tiled = 0,
        Full
    };

    struct NaviMeshBuildParams {
        NaviAgentConfig    agent;
        NaviMeshResolution resolution;
        AABB               bounds;
        uint32_t           version = 1;
    };

    struct NaviMeshTilePayload {
        int32_t              tx    = 0;
        int32_t              ty    = 0;
        uint32_t             layer = 0;
        std::vector<uint8_t> data;
    };

    // Backend-agnostic nav mesh asset payload. Tiled mode carries addressable per-tile blobs plus build
    // params so a pager can load a subset; Full mode carries one monolithic blob.
    struct NaviMeshData {
        NaviMeshExportMode               mode = NaviMeshExportMode::Tiled;
        NaviMeshBuildParams              params;
        std::vector<NaviMeshTilePayload> tiles;
        std::vector<uint8_t>             fullData;
    };

} // namespace sky::ai
