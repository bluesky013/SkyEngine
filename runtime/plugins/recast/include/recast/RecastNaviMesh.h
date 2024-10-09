//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>
#include <DetourNavMesh.h>

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

    struct RecastMeshTileData {
        uint32_t layerIndex = 0;
        AABB     boundBox;
        uint8_t* navData = nullptr;
        uint32_t navDataSize = 0;
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

    class RecastNaviMapFactory : public NaviMeshFactory::Impl {
    public:
        RecastNaviMapFactory() = default;
        ~RecastNaviMapFactory() override = default;

        NaviMesh* CreateNaviMesh() override
        {
            return new RecastNaviMesh();
        }
    };

} // namespace sky::ai