//
// Created on 2026/09/22.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

namespace sky::terrain {

    enum class TerrainHeightFormat : uint8_t {
        R16_UNORM  = 0,
        R32_SFLOAT = 1,
    };

    struct TerrainTileCoord {
        int32_t x = 0;
        int32_t y = 0;

        bool operator==(const TerrainTileCoord &rhs) const { return x == rhs.x && y == rhs.y; }
        bool operator!=(const TerrainTileCoord &rhs) const { return !(*this == rhs); }
        bool operator<(const TerrainTileCoord &rhs) const { return x != rhs.x ? x < rhs.x : y < rhs.y; }
    };

    // Backend- and render-agnostic terrain layout description.
    struct TerrainMeta {
        uint32_t            tileSize     = 64;   // quads per tile side; vertices per side = tileSize + 1
        float               resolution   = 1.f;  // world meters per vertex
        TerrainHeightFormat heightFormat = TerrainHeightFormat::R16_UNORM;
        float               heightScale  = 256.f;
        float               heightOffset = 0.f;
        uint32_t            tileCountX   = 0;
        uint32_t            tileCountY   = 0;
        uint32_t            layerCount   = 0;    // material layers; 4 layers per RGBA splat tile
        uint32_t            lodCount     = 1;    // per-tile LOD levels; LOD L has tileSize >> L quads
        Vector3             origin       = VEC3_ZERO;

        float    GetTileWorldSize() const { return static_cast<float>(tileSize) * resolution; }
        uint32_t GetTileVertexSize() const { return GetLodVertexSize(0); }
        uint32_t GetTileVertexCount() const { return GetLodVertexCount(0); }
        uint32_t GetTileCount() const { return tileCountX * tileCountY; }
        uint32_t GetSplatTileCount() const { return (layerCount + 3u) / 4u; }

        // LOD chain geometry: LOD L halves the quad count down to a minimum of one quad.
        uint32_t GetLodQuadCount(uint32_t lod) const
        {
            const uint32_t quads = tileSize >> lod;
            return quads > 0 ? quads : 1u;
        }
        uint32_t GetLodVertexSize(uint32_t lod) const { return GetLodQuadCount(lod) + 1u; }
        uint32_t GetLodVertexCount(uint32_t lod) const
        {
            const uint32_t size = GetLodVertexSize(lod);
            return size * size;
        }
        uint32_t GetClampedLod(uint32_t lod) const
        {
            const uint32_t maxLod = lodCount > 0 ? lodCount - 1u : 0u;
            return lod < maxLod ? lod : maxLod;
        }

        // Bytes per stored height sample for the declared format.
        uint32_t GetHeightSampleSize() const
        {
            return heightFormat == TerrainHeightFormat::R16_UNORM ? 2u : 4u;
        }

        // Converts a stored height sample to world height.
        float DecodeHeight(float sample) const { return sample * heightScale + heightOffset; }
    };

    struct TerrainTileInfo {
        TerrainTileCoord coord;
        AABB             bounds;      // world-space bounds (X/Z extent, Y min/max)
        float            minHeight = 0.f;
        float            maxHeight = 0.f;
        uint32_t         lodCount  = 1;   // LOD levels available for this tile
    };

    using TerrainTileManifest = std::vector<TerrainTileInfo>;

} // namespace sky::terrain
