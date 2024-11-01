//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>

class dtNavMesh;

namespace sky::ai {

    struct RecastNaviMapConfig {
        uint32_t maxTiles   = 256;
        uint32_t maxPolys   = 16384;
    };

    struct RecastTile {
        int32_t x;
        int32_t y;
    };

    struct RecastNavData : RefObject {
        RecastNavData() = default;
        ~RecastNavData() override;

        uint8_t *data = nullptr;
        uint32_t size = 0;
    };
    using RecastNavDataPtr = CounterPtr<RecastNavData>;

    struct RecastMeshTileData {
        uint32_t layerIndex = 0;
        AABB     boundBox;
        RecastNavDataPtr navData;
    };

    class RecastNaviMesh : public NaviMesh {
    public:
        RecastNaviMesh() = default;
        ~RecastNaviMesh() override;

        bool BuildNavMesh(const RecastNaviMapConfig &config);
        dtNavMesh* GetNavMesh() const { return navMesh; }
    private:
        void ResetNavMesh();

        dtNavMesh *navMesh = nullptr;
    };
} // namespace sky::ai