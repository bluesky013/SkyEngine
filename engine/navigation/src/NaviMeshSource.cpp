//
// Created on 2026/09/21.
//

#include <navigation/NaviMeshSource.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::ai {

    void NaviMeshSourceData::Reflect(SerializationContext *context)
    {
        context->Register<NaviMeshSourceData>("NaviMeshSourceData")
            .Member<&NaviMeshSourceData::scene>("scene")
            .Member<&NaviMeshSourceData::agentHeight>("agentHeight")
            .Member<&NaviMeshSourceData::agentRadius>("agentRadius")
            .Member<&NaviMeshSourceData::agentMaxSlope>("agentMaxSlope")
            .Member<&NaviMeshSourceData::agentMaxClimb>("agentMaxClimb")
            .Member<&NaviMeshSourceData::cellSize>("cellSize")
            .Member<&NaviMeshSourceData::cellHeight>("cellHeight")
            .Member<&NaviMeshSourceData::tileSize>("tileSize")
            .Member<&NaviMeshSourceData::maxSimplificationError>("maxSimplificationError")
            .Member<&NaviMeshSourceData::boundsMinX>("boundsMinX")
            .Member<&NaviMeshSourceData::boundsMinY>("boundsMinY")
            .Member<&NaviMeshSourceData::boundsMinZ>("boundsMinZ")
            .Member<&NaviMeshSourceData::boundsMaxX>("boundsMaxX")
            .Member<&NaviMeshSourceData::boundsMaxY>("boundsMaxY")
            .Member<&NaviMeshSourceData::boundsMaxZ>("boundsMaxZ")
            .Member<&NaviMeshSourceData::exportMode>("exportMode");
    }

} // namespace sky::ai
