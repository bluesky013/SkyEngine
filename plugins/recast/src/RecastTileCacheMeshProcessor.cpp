//
// Created by blues on 2024/10/29.
//

#include <recast/RecastTileCacheMeshProcessor.h>
#include <DetourNavMeshBuilder.h>
#include <DetourTileCacheBuilder.h>

namespace sky::ai {

    void RecastTileCacheMeshProcessor::process(struct dtNavMeshCreateParams* params, uint8_t *polyAreas, uint16_t *polyFlags)
    {
        for (int i = 0; i < params->polyCount; ++i)
        {
            if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA) {
                polyAreas[i] = 0;
            }

            polyFlags[i] = 1;
        }
    }

} // namespace sky::ai