//
// Created by blues on 2024/10/29.
//

#pragma once

#include <DetourTileCache.h>
#include <cstdint>

namespace sky::ai {

    class RecastTileCacheMeshProcessor : public dtTileCacheMeshProcess {
    public:
        RecastTileCacheMeshProcessor() = default;
        ~RecastTileCacheMeshProcessor() override = default;

    private:
        void process(struct dtNavMeshCreateParams* params, uint8_t *polyAreas, uint16_t *polyFlags) override;
    };

} // namespace sky::ai
