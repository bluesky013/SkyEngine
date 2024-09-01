//
// Created by blues on 2024/9/1.
//

#pragma once

#include <navigation/NaviMap.h>
#include <DetourNavMesh.h>

namespace sky::ai {

    class RecastNaviMap : public NaviMap {
    public:
        RecastNaviMap() = default;
        ~RecastNaviMap() override = default;

    private:
        void BuildNavMesh();
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