//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>
#include <recast/RecastConstants.h>
#include <recast/RecastDebugDraw.h>

class dtNavMesh;
class dtNavMeshQuery;
class dtTileCache;

namespace sky {
    class World;
} // namespace sky

namespace sky::ai {

    struct RecastNaviMapConfig {
        uint32_t maxTiles   = RECAST_MAX_BUILD_TILES;
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
        RecastNavDataPtr navData;
    };

    class RecastNaviMesh : public NaviMesh {
    public:
        RecastNaviMesh();
        ~RecastNaviMesh() override;

        bool BuildNavMesh(const RecastNaviMapConfig &config);
        void BuildNavQuery();

        dtNavMesh* GetNavMesh() const { return navMesh; }

        bool Serialize(std::vector<uint8_t> &out) const;
        bool Deserialize(const std::vector<uint8_t> &in);

        NaviQueryResult FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr& filter, const NaviPathQueryParam &param) const override;
        void BuildDebugGeometry(NaviDebugGeometry &out) const override;
        bool LoadData(const NaviMeshData &data) override;
        bool RemoveTile(const NaviMeshTileCoord &coord) override;

    private:
        void ResetNavMesh();

        dtNavMesh *navMesh = nullptr;
        dtNavMeshQuery* navQuery = nullptr;
        dtTileCache *tileCache = nullptr;
    };
} // namespace sky::ai