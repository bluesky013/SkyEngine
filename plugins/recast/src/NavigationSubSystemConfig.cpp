//
// Created on 2026/10/30.
//

#include <recast/NavigationSubSystemConfig.h>

#include <framework/serialization/Any.h>
#include <framework/serialization/PropertyCommon.h>

#include <cmath>
#include <limits>

namespace sky::ai {
    bool NavigationSubSystemConfig::MakeSettings(NaviAgentConfig &agent, NaviMeshResolution &resolution, uint32_t &capacity, std::string &error) const
    {
        if (!std::isfinite(cellSize) || cellSize <= 0 || !std::isfinite(cellHeight) || cellHeight <= 0 || !std::isfinite(tileSize) ||
            tileSize < cellSize || tileSize / cellSize > 255.f || 12.f / cellSize > static_cast<float>(std::numeric_limits<int>::max() / 2) ||
            !std::isfinite(agentHeight) || agentHeight <= 0 || !std::isfinite(agentRadius) || agentRadius < 0 || !std::isfinite(agentMaxSlope) ||
            agentMaxSlope < 0 || agentMaxSlope >= 90 || !std::isfinite(agentMaxClimb) || agentMaxClimb < 0 || agentHeight / cellHeight > 65535.f ||
            agentRadius / cellSize > 255.f || agentMaxClimb / cellHeight > 65535.f || maxTiles <= 0 || maxTiles > (1 << 20)) {
            error = "Invalid navigation resolution, agent dimensions, slope or tile capacity";
            return false;
        }
        agent      = {agentHeight, agentRadius, agentMaxSlope, agentMaxClimb};
        resolution = {cellSize, cellHeight, tileSize};
        capacity   = static_cast<uint32_t>(maxTiles);
        error.clear();
        return true;
    }
    void NavigationSubSystemConfig::Reflect(SerializationContext *context)
    {
        context->Register<NavigationSubSystemConfig>("NavigationSubSystemConfig")
            .Member<&NavigationSubSystemConfig::navMeshAsset>("navMeshAsset")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("NaviMesh")))
            .Member<&NavigationSubSystemConfig::streaming>("streaming")
            .Member<&NavigationSubSystemConfig::cellSize>("cellSize")
            .Member<&NavigationSubSystemConfig::cellHeight>("cellHeight")
            .Member<&NavigationSubSystemConfig::tileSize>("tileSize")
            .Member<&NavigationSubSystemConfig::agentHeight>("agentHeight")
            .Member<&NavigationSubSystemConfig::agentRadius>("agentRadius")
            .Member<&NavigationSubSystemConfig::agentMaxSlope>("agentMaxSlope")
            .Member<&NavigationSubSystemConfig::agentMaxClimb>("agentMaxClimb")
            .Member<&NavigationSubSystemConfig::maxTiles>("maxTiles");
    }
} // namespace sky::ai
