//
// Created by blues on 2024/12/1.
//

#include <terrain/editor/TerrainGenerator.h>
#include <PerlinNoise.hpp>
#include <terrain/TerrainUtils.h>
#include <core/math/MathUtil.h>
#include <core/util/Time.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky::editor {

    void TerrainTileGenerator::Setup(const TileConfig &cfg)
    {
        tileCfg = cfg;
    }

    void TerrainTileGenerator::SetPrefix(const std::string &prefix)
    {
        generatePrefix = prefix;
    }

    void TerrainTileGenerator::SetReceiver(const TileReceiverPtr &ptr)
    {
        receiver = ptr;
    }

    float TerrainTileGenerator::VisitMipData(float *data, uint16_t width, uint32_t i, uint32_t j)
    {
        uint32_t index = i * width + j;
        return data[index];
    }

    void TerrainTileGenerator::GenerateHeightMip(uint32_t mip, uint32_t width) // NOLINT
    {
        uint32_t currentMipWidth = width / 2;
        if (currentMipWidth == 0) {
            return;
        }
        ++mip;

        uint32_t lastOffset = imageData.slices.back().offset;
        auto currentOffset = static_cast<uint32_t>(imageData.rawData.storage.size());

        uint32_t mipSize = currentMipWidth * currentMipWidth * static_cast<uint32_t>(sizeof(float));
        imageData.rawData.storage.resize(imageData.rawData.storage.size() + mipSize);
        
        uint8_t  *rawDataStart = imageData.rawData.storage.data();
        auto *lastMip      = reinterpret_cast<float *>(rawDataStart + lastOffset);
        auto *currentMip   = reinterpret_cast<float *>(rawDataStart + currentOffset);

        for (uint32_t i = 0; i < currentMipWidth; ++i) {
            for (uint32_t j = 0; j < currentMipWidth; ++j) {
                float v1 = VisitMipData(lastMip, width, 2 * i + 0, 2 * j + 0);
                float v2 = VisitMipData(lastMip, width, 2 * i + 0, 2 * j + 1);
                float v3 = VisitMipData(lastMip, width, 2 * i + 1, 2 * j + 1);
                float v4 = VisitMipData(lastMip, width, 2 * i + 1, 2 * j + 0);
        
                uint32_t index = i * currentMipWidth + j;
                currentMip[index] = std::max(std::max(v1, v2), std::max(v3, v4));
            }
        }
        imageData.slices.emplace_back(ImageSliceHeader{currentOffset, mipSize, mip, 0});

        GenerateHeightMip(mip, currentMipWidth);
    }

    void TerrainTileGenerator::OnComplete(bool result)
    {
        if (auto ptr = receiver.lock(); ptr) {
            ptr->results.emplace_back(TerrainSectionData{
                tileCfg.coord, heightMapSource->uuid
            });
        }
    }

    bool TerrainTileGenerator::DoWork()
    {
        const siv::PerlinNoise::seed_type seed = 113344u;
        const siv::PerlinNoise perlin{ seed };

        auto xOffset = static_cast<float>(tileCfg.coord.x * tileCfg.sectionSize);
        auto yOffset = static_cast<float>(tileCfg.coord.y * tileCfg.sectionSize);

        auto texelSize = static_cast<uint32_t>(sizeof(float));
        uint32_t texelNum  = tileCfg.heightMapSize * tileCfg.heightMapSize;
        uint32_t imageSize = texelSize * texelNum;

        imageData.version   = 0;
        imageData.format    = rhi::PixelFormat::R32_SFLOAT;
        imageData.type      = TextureType::TEXTURE_2D;
        imageData.width     = tileCfg.heightMapSize;
        imageData.height    = tileCfg.heightMapSize;
        imageData.mipLevels = CeilLog2(tileCfg.heightMapSize) + 1;
        
        imageData.rawData.storage.resize(imageSize);
        auto *heightMapData = reinterpret_cast<float *>(imageData.rawData.storage.data());
        float scaleFactor = static_cast<float>(tileCfg.sectionSize) / static_cast<float>(tileCfg.heightMapSize);
        for (uint32_t i = 0; i < tileCfg.heightMapSize; ++i) {
            for (uint32_t j = 0; j < tileCfg.heightMapSize; ++j) {
                float y = xOffset + static_cast<float>(i) * scaleFactor;
                float x = yOffset + static_cast<float>(j) * scaleFactor;

                uint32_t index = i * tileCfg.heightMapSize + j;
                auto val = perlin.noise2D_01(x * 0.05, y * 0.05);
                heightMapData[index] = static_cast<float>(val);
            }
        }
        imageData.slices.emplace_back(ImageSliceHeader{0, imageSize, 0, 0});
        GenerateHeightMip(0, tileCfg.heightMapSize);
        imageData.dataSize = static_cast<uint32_t>(imageData.rawData.storage.size());

        AssetSourcePath sourcePath = {};

        std::stringstream ss;
        ss << generatePrefix << "/" << tileCfg.coord.x << "_" << tileCfg.coord.y << ".image";

        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path   = ss.str();

        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
        auto archive = file->WriteAsArchive();
        BinaryOutputArchive bin(*archive);
        imageData.Save(bin);

        heightMapSource = AssetDataBase::Get()->RegisterAsset(sourcePath);
        return true;
    }

    TerrainGenerator::TerrainGenerator()
    {
        binder.Bind(this);
    }

    void TerrainGenerator::Tick(float time)
    {
        if (receiver && receiver->IsDone()) {
            receiver->HeightMapChanged();
            receiver = nullptr;
        }
    }

    void TerrainGenerator::Run(TerrainComponent* component)
    {
        SKY_ASSERT(component != nullptr);

        std::string generatePrefix = std::string("Terrain/") + GetCurrentTimeString();

        auto path = AssetDataBase::Get()->GetWorkSpaceFs()->GetPath();
        path /= generatePrefix;
        path.MakeDirectory();

        const auto &data = component->GetData();

        receiver = std::make_shared<TerrainTileReceiver>();
        receiver->component = component;
        receiver->tileNum = static_cast<uint32_t>(data.sections.size());

        for (const auto &section : data.sections) {
            TerrainTileGenerator::TileConfig tileCfg = {};

            tileCfg.coord         = section.coord;
            tileCfg.sectionSize   = ConvertSectionSize(data.sectionSize);
            tileCfg.heightMapSize = 256;

            CounterPtr<TerrainTileGenerator> tileGen = new TerrainTileGenerator();
            tileGen->SetPrefix(generatePrefix);
            tileGen->SetReceiver(receiver);
            tileGen->Setup(tileCfg);
            tileGen->StartAsync();
        }
    }

    void TerrainTileReceiver::HeightMapChanged()
    {
        component->UpdateHeightMap(std::move(results));
    }
} // namespace sky::editor