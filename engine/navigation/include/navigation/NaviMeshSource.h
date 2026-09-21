//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::ai {

    // Authored `.navmesh` source: references a scene to cook and the params/export mode to use.
    struct NaviMeshSourceData {
        std::string scene;

        float agentHeight   = 1.8f;
        float agentRadius   = 0.4f;
        float agentMaxSlope = 45.f;
        float agentMaxClimb = 0.3f;

        float cellSize   = 0.25f;
        float cellHeight = 0.3f;
        float tileSize   = 10.f;
        float maxSimplificationError = 1.3f;

        float boundsMinX = -50.f;
        float boundsMinY = -50.f;
        float boundsMinZ = -50.f;
        float boundsMaxX = 50.f;
        float boundsMaxY = 50.f;
        float boundsMaxZ = 50.f;

        uint32_t exportMode = 0; // 0 = Tiled, 1 = Full

        static void Reflect(SerializationContext *context);
    };

} // namespace sky::ai
