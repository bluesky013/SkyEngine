//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>
#include <terrain/TerrainAsset.h>
#include <terrain/TerrainSource.h>

#include <core/async/Task.h>

#include <atomic>

namespace sky::terrain {

    // Deterministic world-space normalized height in [0, 1] from the layered noise config.
    float SampleHeight01(const TerrainGenerateConfig &config, float worldX, float worldZ);

    // Generates one tile's full LOD chain (LOD0 + downsampled levels) deterministically.
    TerrainTilePayload GenerateTerrainTile(const TerrainGenerateConfig &config, const TerrainMeta &meta,
                                           const TerrainTileCoord &coord);

    // Off-thread generation task producing a tile payload; shared by the builder and runtime on-demand paths.
    class TerrainTileGenerateTask : public Task {
    public:
        void Setup(const TerrainGenerateConfig *inConfig, const TerrainMeta *inMeta, const TerrainTileCoord &inCoord)
        {
            config = inConfig;
            meta   = inMeta;
            coord  = inCoord;
        }

        const TerrainTilePayload &GetPayload() const { return payload; }
        const TerrainTileCoord   &GetCoord() const { return coord; }
        bool                      IsFinished() const { return finished.load(); }

    protected:
        bool DoWork() override;

    private:
        const TerrainGenerateConfig *config = nullptr;
        const TerrainMeta           *meta   = nullptr;
        TerrainTileCoord             coord;
        TerrainTilePayload           payload;
        std::atomic_bool             finished{false};
    };

} // namespace sky::terrain
