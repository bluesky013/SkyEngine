//
// Created by blues on 2024/12/1.
//

#pragma once

#include <core/async/Task.h>
#include <terrain/TerrainBase.h>
#include <terrain/TerrainComponent.h>
#include <render/adaptor/assets/ImageAsset.h>

namespace sky::editor {

    class TerrainTileGenerator : public Task {
    public:
        TerrainTileGenerator() = default;
        ~TerrainTileGenerator() override = default;

        struct TileConfig {
            int32_t x;
            int32_t y;

            uint32_t heightMapSize;
            uint32_t sectionSize;
        };

        void Setup(const TileConfig &cfg);
        void SetPrefix(const std::string &prefix);
    private:
        bool DoWork() override;
        void GenerateHeightMip(uint32_t mip, uint32_t width);
        static uint16_t VisitMipData(uint16_t *data, uint16_t width, uint32_t i, uint32_t j);

        TileConfig tileCfg = {};
        ImageAssetData imageData;

        std::string generatePrefix;
        AssetSourcePtr heightMapSource;
    };
} // namespace sky::editor
