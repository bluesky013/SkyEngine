//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>

class dtNavMesh;

namespace sky::ai {

    struct RecastNaviMapConfig {
        Vector3  origin = VEC3_ZERO;
        float    tileWidth  = 16;
        float    tileHeight = 16;
        uint32_t maxTiles   = 65536;
        uint32_t maxPolys   = 65536;
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
        ~RecastNaviMesh() override = default;

        void BuildNavMesh(const RecastNaviMapConfig &config);
    private:
        void ResetNavMesh();

        dtNavMesh *navMesh = nullptr;
    };
} // namespace sky::ai