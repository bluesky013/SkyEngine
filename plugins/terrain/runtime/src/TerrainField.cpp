//
// Created on 2026/09/22.
//

#include <terrain/TerrainField.h>
#include <terrain/TerrainAddress.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sky::terrain {

    float TerrainField::DecodeSample(const uint8_t *data, uint32_t index) const
    {
        if (meta.heightFormat == TerrainHeightFormat::R16_UNORM) {
            uint16_t raw = 0;
            std::memcpy(&raw, data + static_cast<size_t>(index) * sizeof(uint16_t), sizeof(uint16_t));
            return (static_cast<float>(raw) / 65535.f) * meta.heightScale + meta.heightOffset;
        }

        float raw = 0.f;
        std::memcpy(&raw, data + static_cast<size_t>(index) * sizeof(float), sizeof(float));
        return raw * meta.heightScale + meta.heightOffset;
    }

    bool TerrainField::AddTile(const TerrainTileCoord &coord, const TerrainLodPayload &lodPayload)
    {
        const uint32_t vertexCount = meta.GetTileVertexCount();  // LOD0, the query detail level
        const uint32_t sampleSize  = meta.GetHeightSampleSize();
        if (vertexCount == 0 || lodPayload.height.size() < static_cast<size_t>(vertexCount) * sampleSize) {
            return false;
        }

        StoredTile stored;
        stored.heights.resize(vertexCount);
        for (uint32_t i = 0; i < vertexCount; ++i) {
            stored.heights[i] = DecodeSample(lodPayload.height.data(), i);
        }
        stored.splat = lodPayload.splat;

        tiles[coord] = std::move(stored);
        return true;
    }

    bool TerrainField::RemoveTile(const TerrainTileCoord &coord)
    {
        return tiles.erase(coord) > 0;
    }

    bool TerrainField::HasTile(const TerrainTileCoord &coord) const
    {
        return tiles.find(coord) != tiles.end();
    }

    void TerrainField::Clear()
    {
        tiles.clear();
    }

    bool TerrainField::GetTileHeights(const TerrainTileCoord &coord, const float *&outHeights, uint32_t &outVertexSize) const
    {
        const auto *tile = FindTile(coord);
        if (tile == nullptr || tile->heights.empty()) {
            return false;
        }
        outHeights    = tile->heights.data();
        outVertexSize = meta.GetTileVertexSize();
        return true;
    }

    const TerrainField::StoredTile *TerrainField::FindTile(const TerrainTileCoord &coord) const
    {
        const auto iter = tiles.find(coord);
        return iter == tiles.end() ? nullptr : &iter->second;
    }

    bool TerrainField::SampleGrid(const StoredTile &tile, float localX, float localZ, float &outHeight) const
    {
        const uint32_t size = meta.GetTileVertexSize();
        if (size < 2 || tile.heights.size() < static_cast<size_t>(size) * size) {
            return false;
        }

        const float maxIndex = static_cast<float>(size - 1);
        localX = std::min(std::max(localX, 0.f), maxIndex);
        localZ = std::min(std::max(localZ, 0.f), maxIndex);

        uint32_t ix = static_cast<uint32_t>(localX);
        uint32_t iz = static_cast<uint32_t>(localZ);
        if (ix >= size - 1) { ix = size - 2; }
        if (iz >= size - 1) { iz = size - 2; }

        const float fx = localX - static_cast<float>(ix);
        const float fz = localZ - static_cast<float>(iz);

        auto at = [&](uint32_t x, uint32_t z) { return tile.heights[static_cast<size_t>(z) * size + x]; };

        const float h00 = at(ix, iz);
        const float h10 = at(ix + 1, iz);
        const float h01 = at(ix, iz + 1);
        const float h11 = at(ix + 1, iz + 1);

        const float h0 = h00 + (h10 - h00) * fx;
        const float h1 = h01 + (h11 - h01) * fx;
        outHeight = h0 + (h1 - h0) * fz;
        return true;
    }

    bool TerrainField::QueryHeight(const Vector3 &worldPos, float &outHeight) const
    {
        const auto coord  = WorldToTile(meta, worldPos);
        const auto *tile  = FindTile(coord);
        if (tile == nullptr) {
            return false;
        }

        float localX = 0.f;
        float localZ = 0.f;
        WorldToLocalTexel(meta, worldPos, localX, localZ);
        return SampleGrid(*tile, localX, localZ, outHeight);
    }

    bool TerrainField::QueryNormal(const Vector3 &worldPos, Vector3 &outNormal) const
    {
        const float d = meta.resolution > 0.f ? meta.resolution : 1.f;

        float hL = 0.f;
        float hR = 0.f;
        float hB = 0.f;
        float hF = 0.f;
        if (!QueryHeight(Vector3(worldPos.x - d, worldPos.y, worldPos.z), hL) ||
            !QueryHeight(Vector3(worldPos.x + d, worldPos.y, worldPos.z), hR) ||
            !QueryHeight(Vector3(worldPos.x, worldPos.y, worldPos.z - d), hB) ||
            !QueryHeight(Vector3(worldPos.x, worldPos.y, worldPos.z + d), hF)) {
            return false;
        }

        const float gx = (hR - hL) / (2.f * d);
        const float gz = (hF - hB) / (2.f * d);

        outNormal = Vector3(-gx, 1.f, -gz);
        outNormal.Normalize();
        return true;
    }

    bool TerrainField::QuerySplatWeights(const Vector3 &worldPos, Vector4 &outWeights) const
    {
        const auto coord = WorldToTile(meta, worldPos);
        const auto *tile = FindTile(coord);
        if (tile == nullptr || tile->splat.empty()) {
            return false;
        }

        const auto &splat = tile->splat[0];
        const uint32_t size = meta.GetTileVertexSize();
        if (splat.size() < static_cast<size_t>(size) * size * 4u) {
            return false;
        }

        float localX = 0.f;
        float localZ = 0.f;
        WorldToLocalTexel(meta, worldPos, localX, localZ);

        const float maxIndex = static_cast<float>(meta.tileSize);
        const uint32_t ix = static_cast<uint32_t>(std::min(std::max(localX, 0.f), maxIndex));
        const uint32_t iz = static_cast<uint32_t>(std::min(std::max(localZ, 0.f), maxIndex));
        const size_t   base = (static_cast<size_t>(iz) * size + ix) * 4u;

        outWeights = Vector4(
            static_cast<float>(splat[base + 0]) / 255.f,
            static_cast<float>(splat[base + 1]) / 255.f,
            static_cast<float>(splat[base + 2]) / 255.f,
            static_cast<float>(splat[base + 3]) / 255.f);
        return true;
    }

    bool TerrainField::Raycast(const Vector3 &origin, const Vector3 &dir, float maxDist, TerrainRaycastHit &out) const
    {
        if (tiles.empty() || maxDist <= 0.f) {
            return false;
        }

        const float length = dir.Length();
        if (length <= 0.f) {
            return false;
        }
        const Vector3 direction = dir / length;

        const float step = std::max(meta.resolution * 0.5f, 0.25f);

        float prevT    = 0.f;
        bool  havePrev = false;

        for (float t = 0.f; t <= maxDist; t += step) {
            const Vector3 point = origin + direction * t;
            float height = 0.f;
            if (!QueryHeight(point, height)) {
                havePrev = false;
                continue;
            }

            if (point.y <= height) {
                float lo = havePrev ? prevT : t;
                float hi = t;
                for (int i = 0; i < 8 && hi > lo; ++i) {
                    const float mid = (lo + hi) * 0.5f;
                    const Vector3 midPoint = origin + direction * mid;
                    float midHeight = 0.f;
                    if (!QueryHeight(midPoint, midHeight)) {
                        break;
                    }
                    if (midPoint.y <= midHeight) {
                        hi = mid;
                    } else {
                        lo = mid;
                    }
                }

                out.distance = hi;
                out.position = origin + direction * hi;
                out.normal   = VEC3_Y;
                Vector3 normal;
                if (QueryNormal(out.position, normal)) {
                    out.normal = normal;
                }
                return true;
            }

            prevT    = t;
            havePrev = true;
        }

        return false;
    }

} // namespace sky::terrain
