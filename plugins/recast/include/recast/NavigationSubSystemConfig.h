//
// Created on 2026/10/07.
//

#pragma once

#include <framework/serialization/SerializationContext.h>

namespace sky::ai {

    // Authoring config for the navigation world subsystem (shown in the editor's
    // project-level Config panel).
    struct NavigationSubSystemConfig {
        float cellSize      = 0.3f;
        float agentHeight   = 2.0f;
        float agentRadius   = 0.6f;
        float agentMaxSlope = 45.0f; // degrees
        float agentMaxClimb = 0.4f;
        int   maxTiles      = 256;

        static void Reflect(SerializationContext *context);
    };

} // namespace sky::ai
