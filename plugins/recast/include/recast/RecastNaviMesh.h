//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Vector3.h>
#include <navigation/NaviMesh.h>
#include <recast/RecastConstants.h>
#include <recast/RecastLz4Compressor.h>
#include <recast/RecastTileCacheMeshProcessor.h>

#include <DetourTileCacheBuilder.h>

#include <shared_mutex>

class dtNavMesh;
class dtTileCache;

namespace sky {
    class World;
} // namespace sky

namespace sky::ai {

    struct RecastNaviMapConfig {
        uint32_t maxTiles = RECAST_MAX_BUILD_TILES;
        uint32_t maxPolys = 16384;
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
        bool BuildNavQuery();

        // Borrowed native access requires external exclusion of queries and mesh mutation.
        dtNavMesh *GetNavMesh() const
        {
            return navMesh;
        }

        bool Serialize(std::vector<uint8_t> &out) const;
        bool Deserialize(const std::vector<uint8_t> &in);
        bool ApplyBuild(const NaviMeshData &data, const std::vector<NaviMeshTileCoord> &rebuiltTiles) override;
        bool SaveFullData(std::vector<uint8_t> &out) const override
        {
            return Serialize(out);
        }

        NaviQueryResult
        FindPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &param) const override;
        NaviQueryResult QueryPath(const Vector3            &start,
                                  const Vector3            &end,
                                  const NaviQueryFilterPtr &filter,
                                  const NaviPathQueryParam &param,
                                  NaviPath                 &out) const override;
        void            BuildDebugGeometry(NaviDebugGeometry &out) const override;
        bool            LoadData(const NaviMeshData &data) override;
        bool            AddTile(const NaviMeshTilePayload &tile) override;
        bool            RemoveTile(const NaviMeshTileCoord &coord) override;
        bool            PrepareStreaming(const NaviMeshBuildParams &params) override;

    private:
        void ResetNavMesh();
        bool BuildNavMeshUnlocked(const RecastNaviMapConfig &config);
        bool BuildNavQueryUnlocked();
        bool PrepareStreamingUnlocked(const NaviMeshBuildParams &params);
        bool AddTileUnlocked(const NaviMeshTilePayload &tile);
        bool RemoveTileUnlocked(const NaviMeshTileCoord &coord);
        bool SerializeUnlocked(std::vector<uint8_t> &out) const;
        bool DeserializeUnlocked(const std::vector<uint8_t> &in);
        bool InitTileCache(const NaviMeshBuildParams &params);

        dtNavMesh   *navMesh   = nullptr;
        dtTileCache *tileCache = nullptr;

        // Per-instance state: worlds must not share allocator / mesh processor state.
        NaviCompressor               compressor;
        dtTileCacheAlloc             tileCacheAlloc;
        RecastTileCacheMeshProcessor meshProcessor;

        // Readers own separate search contexts; native tile/link mutation excludes every reader.
        mutable std::shared_mutex queryMutex;
    };
} // namespace sky::ai
