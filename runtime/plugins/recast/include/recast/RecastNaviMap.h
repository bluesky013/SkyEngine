//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMap.h>
#include <DetourNavMesh.h>

namespace sky::ai {

    struct RecastNaviMapConfig {
        Vector3  origin = VEC3_ZERO;
        float    tileWidth  = 16;
        float    tileHeight = 16;
        uint32_t maxTiles   = 65536;
        uint32_t maxPolys   = 65536;
    };

    class RecastNaviMap : public NaviMap {
    public:
        RecastNaviMap() = default;
        ~RecastNaviMap() override = default;

        void BuildNavMesh(const RecastNaviMapConfig &config);
    private:
        void ResetNavMesh();

        dtNavMesh *navMesh = nullptr;
    };

    class RecastNaviMapFactory : public NaviMapFactory::Impl {
    public:
        RecastNaviMapFactory() = default;
        ~RecastNaviMapFactory() override = default;

        NaviMap* CreateNaviMap() override
        {
            return new RecastNaviMap();
        }
    };

} // namespace sky::ai