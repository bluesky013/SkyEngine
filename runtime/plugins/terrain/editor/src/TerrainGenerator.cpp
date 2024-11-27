//
// Created by blues on 2024/12/1.
//

#include <terrain/editor/TerrainGenerator.h>
#include <PerlinNoise.hpp>
#include <terrain/TerrainUtils.h>
#include <core/math/MathUtil.h>
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

    uint16_t TerrainTileGenerator::VisitMipData(uint16_t *data, uint16_t width, uint32_t i, uint32_t j)
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

        uint32_t mipSize = currentMipWidth * currentMipWidth * static_cast<uint32_t>(sizeof(uint16_t));
        imageData.rawData.storage.resize(imageData.rawData.storage.size() + mipSize);
        
        uint8_t  *rawDataStart = imageData.rawData.storage.data();
        auto *lastMip      = reinterpret_cast<uint16_t *>(rawDataStart + lastOffset);
        auto *currentMip   = reinterpret_cast<uint16_t *>(rawDataStart + currentOffset);

        for (uint32_t i = 0; i < currentMipWidth; ++i) {
            for (uint32_t j = 0; j < currentMipWidth; ++j) {
                uint16_t v1 = VisitMipData(lastMip, width, 2 * i + 0, 2 * j + 0);
                uint16_t v2 = VisitMipData(lastMip, width, 2 * i + 0, 2 * j + 1);
                uint16_t v3 = VisitMipData(lastMip, width, 2 * i + 1, 2 * j + 1);
                uint16_t v4 = VisitMipData(lastMip, width, 2 * i + 1, 2 * j + 0);
        
                uint32_t index = i * currentMipWidth + j;
                currentMip[index] = std::max(std::max(v1, v2), std::max(v3, v4));
            }
        }
        imageData.slices.emplace_back(ImageSliceHeader{currentOffset, mipSize, mip, 0});

        GenerateHeightMip(mip, currentMipWidth);
    }

    bool TerrainTileGenerator::DoWork()
    {
        const siv::PerlinNoise::seed_type seed = 113344u;
        const siv::PerlinNoise perlin{ seed };

        auto xOffset = static_cast<float>(tileCfg.x * tileCfg.sectionSize);
        auto yOffset = static_cast<float>(tileCfg.y * tileCfg.sectionSize);

        auto texelSize = static_cast<uint32_t>(sizeof(uint16_t));
        uint32_t texelNum  = tileCfg.heightMapSize * tileCfg.heightMapSize;
        uint32_t imageSize = texelSize * texelNum;

        imageData.version   = 0;
        imageData.format    = rhi::PixelFormat::R16_UNORM;
        imageData.type      = TextureType::TEXTURE_2D;
        imageData.width     = tileCfg.heightMapSize;
        imageData.height    = tileCfg.heightMapSize;
        imageData.mipLevels = CeilLog2(tileCfg.heightMapSize);
        
        imageData.rawData.storage.resize(imageSize);
        auto *heightMapData = reinterpret_cast<uint16_t *>(imageData.rawData.storage.data());
        float scaleFactor = static_cast<float>(tileCfg.sectionSize) / static_cast<float>(tileCfg.heightMapSize);
        for (uint32_t i = 0; i < tileCfg.heightMapSize; ++i) {
            for (uint32_t j = 0; j < tileCfg.heightMapSize; ++j) {
                float x = xOffset + static_cast<float>(i) * scaleFactor;
                float y = yOffset + static_cast<float>(j) * scaleFactor;

                uint32_t index = i * tileCfg.heightMapSize + j;
                auto val = perlin.noise2D_01(x, y);
                heightMapData[index] = static_cast<uint16_t>(val * 65536.0);
            }
        }
        imageData.slices.emplace_back(ImageSliceHeader{0, imageSize, 0, 0});
        GenerateHeightMip(0, tileCfg.heightMapSize);
        imageData.dataSize = static_cast<uint32_t>(imageData.rawData.storage.size());

        AssetSourcePath sourcePath = {};

        std::stringstream ss;
        ss << generatePrefix << "/" << tileCfg.x << "_" << tileCfg.y << ".image";

        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path   = ss.str();

        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
        auto archive = file->WriteAsArchive();
        BinaryOutputArchive bin(*archive);
        imageData.Save(bin);

        heightMapSource = AssetDataBase::Get()->RegisterAsset(sourcePath);
        return true;
    }
} // namespace sky::editor