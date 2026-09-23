//
// Created on 2026/09/22.
//

#include <terrain/TerrainAssetBuilder.h>

#include <terrain/TerrainAddress.h>
#include <terrain/TerrainGenerator.h>

#include <algorithm>
#include <cfloat>

namespace sky::terrain {

    namespace {

        void ComputeMinMax(const TerrainMeta &meta, const TerrainLodPayload &lod, float &minHeight, float &maxHeight)
        {
            minHeight = FLT_MAX;
            maxHeight = -FLT_MAX;

            const uint32_t count = meta.GetTileVertexCount();
            if (meta.heightFormat == TerrainHeightFormat::R32_SFLOAT) {
                const auto *samples = reinterpret_cast<const float *>(lod.height.data());
                for (uint32_t i = 0; i < count; ++i) {
                    minHeight = std::min(minHeight, samples[i]);
                    maxHeight = std::max(maxHeight, samples[i]);
                }
            } else {
                const auto *samples = reinterpret_cast<const uint16_t *>(lod.height.data());
                for (uint32_t i = 0; i < count; ++i) {
                    const float h = (static_cast<float>(samples[i]) / 65535.f) * meta.heightScale + meta.heightOffset;
                    minHeight = std::min(minHeight, h);
                    maxHeight = std::max(maxHeight, h);
                }
            }
        }

    } // namespace

    bool BuildTerrainAsset(const TerrainSourceData &source, TerrainAssetData &out)
    {
        TerrainMeta          meta;
        TerrainGenerateConfig config;
        ToGenerateConfig(source, meta, config);

        if (meta.tileSize == 0 || config.lodCount == 0) {
            return false;
        }

        out      = TerrainAssetData{};
        out.meta = meta;

        const float tileWorld = meta.GetTileWorldSize();

        for (uint32_t y = 0; y < source.tileCountY; ++y) {
            for (uint32_t x = 0; x < source.tileCountX; ++x) {
                const TerrainTileCoord coord{
                    source.tileStartX + static_cast<int32_t>(x),
                    source.tileStartY + static_cast<int32_t>(y)
                };

                auto payload = GenerateTerrainTile(config, meta, coord);

                float minHeight = 0.f;
                float maxHeight = 0.f;
                ComputeMinMax(meta, payload.lods[0], minHeight, maxHeight);

                TerrainTileInfo info;
                info.coord     = coord;
                info.lodCount  = config.lodCount;
                info.minHeight = minHeight;
                info.maxHeight = maxHeight;

                const Vector3 origin = TileToWorld(meta, coord);
                info.bounds = AABB(
                    Vector3(origin.x, minHeight, origin.z),
                    Vector3(origin.x + tileWorld, maxHeight, origin.z + tileWorld));

                out.tiles.push_back(std::move(payload));
                out.manifest.push_back(info);
            }
        }

        return true;
    }

} // namespace sky::terrain
