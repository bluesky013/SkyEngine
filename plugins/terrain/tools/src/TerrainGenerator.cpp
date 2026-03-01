//
// Created by blues on 2024/11/13.
//

#include <terrain/tools/TerrainGenerator.h>
#include <PerlinNoise.hpp>

namespace sky {

    void TerrainGenerator::Setup(const Config &cfg)
    {
        config = cfg;
    }

    bool TerrainGenerator::DoWork()
    {
        const siv::PerlinNoise::seed_type seed = 113344u;
        const siv::PerlinNoise perlin{ seed };

        uint32_t blockWidth = 256;
        uint32_t blockNum = config.maxExt / config.minExt;

        auto fn = [&](uint32_t x, uint32_t y) {
            std::stringstream ss;
            std::vector<uint16_t> data(blockWidth * blockWidth, 0);

            uint64_t xOffset = x * blockWidth;
            uint64_t yOffset = y * blockWidth;

            for (uint64_t i = 0; i < blockWidth; ++i) {
                for (uint64_t j = 0; j < blockWidth; ++j) {
                    const double noise       = perlin.noise2D_01((xOffset + i) * 0.002, (yOffset + j) * 0.002);
                    data[j * blockWidth + i] = static_cast<uint16_t>(noise * 65535);
                }
            }
        };

        for (uint32_t i = 0; i < blockNum; ++i) {
            for (uint32_t j = 0; j < blockNum; ++j) {
                fn(i, j);
            }
        }
        return true;
    }
} // namespace sky
