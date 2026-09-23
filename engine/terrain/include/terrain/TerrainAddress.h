//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>

namespace sky::terrain {

    // World position -> tile coordinate containing it.
    TerrainTileCoord WorldToTile(const TerrainMeta &meta, const Vector3 &worldPos);

    // Tile coordinate -> world-space origin (min corner) of that tile.
    Vector3 TileToWorld(const TerrainMeta &meta, const TerrainTileCoord &coord);

    // World position -> in-tile texel offset in [0, tileSize].
    void WorldToLocalTexel(const TerrainMeta &meta, const Vector3 &worldPos, float &localX, float &localZ);

    // Inclusive tile range covering a world-space bounds (X/Z extent).
    void TileRangeForBounds(const TerrainMeta &meta, const AABB &bounds, TerrainTileCoord &minCoord, TerrainTileCoord &maxCoord);

} // namespace sky::terrain
