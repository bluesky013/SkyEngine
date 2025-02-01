//
// Created by blues on 2024/12/1.
//

#pragma once

#include <core/async/Task.h>
#include <framework/interface/ITickEvent.h>
#include <terrain/TerrainBase.h>
#include <terrain/TerrainComponent.h>
#include <render/adaptor/assets/ImageAsset.h>

namespace sky::editor {


    struct TerrainTileReceiver {
        TerrainComponent* component = nullptr;
        std::vector<TerrainSectionData> results;
        uint32_t tileNum = 0;

        void HeightMapChanged();

        bool IsDone() const
        {
            return static_cast<uint32_t>(results.size()) == tileNum;
        }
    };
    using TileReceiverPtr = std::shared_ptr<TerrainTileReceiver>;
    using TileReceiverWeak = std::weak_ptr<TerrainTileReceiver>;

    class TerrainTileGenerator : public Task {
    public:
        TerrainTileGenerator() = default;
        ~TerrainTileGenerator() override = default;

        struct TileConfig {
            TerrainCoord coord;

            uint32_t heightMapSize;
            uint32_t sectionSize;
        };

        void Setup(const TileConfig &cfg);
        void SetPrefix(const std::string &prefix);
        void SetReceiver(const TileReceiverPtr &ptr);
    private:
        bool DoWork() override;
        void OnComplete(bool result) override;

        void GenerateHeightMip(uint32_t mip, uint32_t width);
        static float VisitMipData(float *data, uint16_t width, uint32_t i, uint32_t j);

        TileConfig tileCfg = {};
        ImageAssetData imageData;

        std::string generatePrefix;
        AssetSourcePtr heightMapSource;

        TileReceiverWeak receiver;
    };

    class TerrainGenerator : public ITickEvent {
    public:
        TerrainGenerator();
        ~TerrainGenerator() override = default;

        void Run(TerrainComponent* component);
    private:
        void Tick(float time) override;

        TileReceiverPtr receiver;
        EventBinder<ITickEvent> binder;
    };
} // namespace sky::editor
