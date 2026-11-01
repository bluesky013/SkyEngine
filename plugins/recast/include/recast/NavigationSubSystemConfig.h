//
// Created on 2026/10/30.
//

#pragma once

#include <core/util/Uuid.h>
#include <framework/serialization/SerializationContext.h>
#include <navigation/NaviMesh.h>
#include <string>

namespace sky::ai {
    struct NavigationSubSystemConfig {
        Uuid  navMeshAsset;
        bool  streaming     = false;
        float cellSize      = .3f;
        float cellHeight    = .3f;
        float tileSize      = 10.f;
        float agentHeight   = 2.f;
        float agentRadius   = .6f;
        float agentMaxSlope = 45.f;
        float agentMaxClimb = .4f;
        int   maxTiles      = 256;

        bool        MakeSettings(NaviAgentConfig &agent, NaviMeshResolution &resolution, uint32_t &capacity, std::string &error) const;
        static void Reflect(SerializationContext *context);
    };
} // namespace sky::ai
