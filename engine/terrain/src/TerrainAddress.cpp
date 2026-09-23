//
// Created on 2026/09/22.
//

#include <terrain/TerrainAddress.h>
#include <cmath>

namespace sky::terrain {

    TerrainTileCoord WorldToTile(const TerrainMeta &meta, const Vector3 &worldPos)
    {
        TerrainTileCoord coord;

        const float tileWorldSize = meta.GetTileWorldSize();
        if (tileWorldSize <= 0.f) {
            return coord;
        }

        coord.x = static_cast<int32_t>(std::floor((worldPos.x - meta.origin.x) / tileWorldSize));
        coord.y = static_cast<int32_t>(std::floor((worldPos.z - meta.origin.z) / tileWorldSize));
        return coord;
    }

    Vector3 TileToWorld(const TerrainMeta &meta, const TerrainTileCoord &coord)
    {
        const float tileWorldSize = meta.GetTileWorldSize();
        return {
            meta.origin.x + static_cast<float>(coord.x) * tileWorldSize,
            meta.origin.y,
            meta.origin.z + static_cast<float>(coord.y) * tileWorldSize
        };
    }

    void WorldToLocalTexel(const TerrainMeta &meta, const Vector3 &worldPos, float &localX, float &localZ)
    {
        const float tileSize = static_cast<float>(meta.tileSize);
        const float res      = meta.resolution > 0.f ? meta.resolution : 1.f;

        localX = (worldPos.x - meta.origin.x) / res;
        localZ = (worldPos.z - meta.origin.z) / res;

        localX -= std::floor(localX / tileSize) * tileSize;
        localZ -= std::floor(localZ / tileSize) * tileSize;
    }

    void TileRangeForBounds(const TerrainMeta &meta, const AABB &bounds, TerrainTileCoord &minCoord, TerrainTileCoord &maxCoord)
    {
        minCoord = WorldToTile(meta, Vector3(bounds.min.x, 0.f, bounds.min.z));
        maxCoord = WorldToTile(meta, Vector3(bounds.max.x, 0.f, bounds.max.z));
    }

} // namespace sky::terrain
