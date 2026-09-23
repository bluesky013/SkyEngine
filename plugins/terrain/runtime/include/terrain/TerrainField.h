//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>
#include <terrain/TerrainAsset.h>
#include <terrain/TerrainQuery.h>
#include <terrain/TerrainTileKey.h>

#include <unordered_map>
#include <vector>

namespace sky::terrain {

    // Runtime height field implementation (decoded tile samples plus CPU queries).
    class TerrainField : public ITerrainField {
    public:
        void SetMeta(const TerrainMeta &inMeta) { meta = inMeta; }
        const TerrainMeta &GetMeta() const override { return meta; }

        bool AddTile(const TerrainTileCoord &coord, const TerrainLodPayload &lodPayload);
        bool RemoveTile(const TerrainTileCoord &coord);
        bool HasTile(const TerrainTileCoord &coord) const;
        void Clear();
        uint32_t GetLoadedTileCount() const override { return static_cast<uint32_t>(tiles.size()); }

        // Resident LOD0 height grid for a tile (row-major z * vertexSize + x). False when not loaded.
        bool GetTileHeights(const TerrainTileCoord &coord, const float *&outHeights, uint32_t &outVertexSize) const override;

        // Queries return false when the containing tile is not loaded or has no data.
        bool QueryHeight(const Vector3 &worldPos, float &outHeight) const override;
        bool QueryNormal(const Vector3 &worldPos, Vector3 &outNormal) const override;
        bool QuerySplatWeights(const Vector3 &worldPos, Vector4 &outWeights) const override;
        bool Raycast(const Vector3 &origin, const Vector3 &dir, float maxDist, TerrainRaycastHit &out) const override;

    private:
        struct StoredTile {
            std::vector<float>                heights;  // (tileSize + 1)^2 decoded world heights
            std::vector<std::vector<uint8_t>> splat;    // RGBA8 splat tiles, 4 layers each
        };

        const StoredTile *FindTile(const TerrainTileCoord &coord) const;
        float DecodeSample(const uint8_t *data, uint32_t index) const;
        bool  SampleGrid(const StoredTile &tile, float localX, float localZ, float &outHeight) const;

        TerrainMeta meta;
        std::unordered_map<TerrainTileCoord, StoredTile, TerrainTileHash> tiles;
    };

} // namespace sky::terrain
