//
// Created on 2026/10/07.
//

#include <recast/NavigationSubSystemConfig.h>

namespace sky::ai {

    void NavigationSubSystemConfig::Reflect(SerializationContext *context)
    {
        context->Register<NavigationSubSystemConfig>("NavigationSubSystemConfig")
            .Member<&NavigationSubSystemConfig::cellSize>("cellSize")
            .Member<&NavigationSubSystemConfig::agentHeight>("agentHeight")
            .Member<&NavigationSubSystemConfig::agentRadius>("agentRadius")
            .Member<&NavigationSubSystemConfig::agentMaxSlope>("agentMaxSlope")
            .Member<&NavigationSubSystemConfig::agentMaxClimb>("agentMaxClimb")
            .Member<&NavigationSubSystemConfig::maxTiles>("maxTiles");
    }

} // namespace sky::ai
