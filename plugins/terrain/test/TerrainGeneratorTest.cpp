//
// Created on 2026/09/22.
//

#include <terrain/TerrainGenerator.h>
#include <terrain/TerrainAddress.h>
#include <terrain/TerrainSystem.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

using namespace sky;
using namespace sky::terrain;

namespace {

    TerrainMeta MakeMeta()
    {
        TerrainMeta meta;
        meta.tileSize     = 8;
        meta.resolution   = 1.f;
        meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        meta.heightScale  = 1.f;
        meta.heightOffset = 0.f;
        meta.layerCount   = 2;
        meta.lodCount     = 3;
        return meta;
    }

    TerrainGenerateConfig MakeConfig()
    {
        TerrainGenerateConfig config;
        config.seed             = 1234;
        config.noise.baseFrequency = 0.05f;
        config.noise.octaves    = 4;
        config.heightScale      = 10.f;
        config.heightOffset     = 0.f;
        config.layerCount       = 2;
        config.layers[0]        = TerrainLayerRule{-1000.f, 1000.f, 0.f, 30.f};
        config.layers[1]        = TerrainLayerRule{-1000.f, 1000.f, 30.f, 90.f};
        config.lodCount         = 3;
        return config;
    }

    const float *AsFloats(const TerrainLodPayload &lod)
    {
        return reinterpret_cast<const float *>(lod.height.data());
    }

    void Settle(TerrainSystem &system)
    {
        for (int i = 0; i < 128; ++i) {
            system.Tick(0.f);
            TaskExecutor::Get()->WaitForAll();
            system.Tick(0.f);
            if (system.GetPendingLoadCount() == 0 && system.GetLoadedTileCount() > 0) {
                break;
            }
        }
    }

} // namespace

TEST(TerrainGeneratorTest, Deterministic)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    const auto a = GenerateTerrainTile(config, meta, {0, 0});
    const auto b = GenerateTerrainTile(config, meta, {0, 0});

    EXPECT_EQ(a.lods[0].height, b.lods[0].height);
    EXPECT_EQ(a.lods[0].splat[0], b.lods[0].splat[0]);
    EXPECT_EQ(a.lods[2].height, b.lods[2].height);
}

TEST(TerrainGeneratorTest, OrderIndependent)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    const auto a0 = GenerateTerrainTile(config, meta, {0, 0});
    const auto a1 = GenerateTerrainTile(config, meta, {1, 0});

    const auto b1 = GenerateTerrainTile(config, meta, {1, 0});
    const auto b0 = GenerateTerrainTile(config, meta, {0, 0});

    EXPECT_EQ(a0.lods[0].height, b0.lods[0].height);
    EXPECT_EQ(a1.lods[0].height, b1.lods[0].height);
}

TEST(TerrainGeneratorTest, SeamlessBorders)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    const auto left  = GenerateTerrainTile(config, meta, {0, 0});
    const auto right = GenerateTerrainTile(config, meta, {1, 0});

    const uint32_t size = meta.GetTileVertexSize();
    const float *lf = AsFloats(left.lods[0]);
    const float *rf = AsFloats(right.lods[0]);

    for (uint32_t z = 0; z < size; ++z) {
        EXPECT_NEAR(lf[z * size + (size - 1)], rf[z * size + 0], 1e-6f);
    }
}

TEST(TerrainGeneratorTest, SplatNormalized)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    const auto tile = GenerateTerrainTile(config, meta, {0, 0});
    const auto &splat = tile.lods[0].splat[0];

    const uint32_t size = meta.GetTileVertexSize();
    for (uint32_t z = 0; z < size; ++z) {
        for (uint32_t x = 0; x < size; ++x) {
            const size_t base = (static_cast<size_t>(z) * size + x) * 4u;
            const uint32_t sum = splat[base] + splat[base + 1] + splat[base + 2] + splat[base + 3];
            EXPECT_EQ(sum, 255u);
        }
    }
}

TEST(TerrainGeneratorTest, LodChainHalves)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    const auto tile = GenerateTerrainTile(config, meta, {0, 0});
    ASSERT_EQ(tile.lods.size(), 3u);

    EXPECT_EQ(tile.lods[0].height.size(), static_cast<size_t>(meta.GetLodVertexCount(0)) * sizeof(float));
    EXPECT_EQ(tile.lods[1].height.size(), static_cast<size_t>(meta.GetLodVertexCount(1)) * sizeof(float));
    EXPECT_EQ(tile.lods[2].height.size(), static_cast<size_t>(meta.GetLodVertexCount(2)) * sizeof(float));

    // Averaged coarse samples stay within the fine range.
    const uint32_t n0 = meta.GetLodVertexCount(0);
    float minH = AsFloats(tile.lods[0])[0];
    float maxH = minH;
    for (uint32_t i = 0; i < n0; ++i) {
        const float h = AsFloats(tile.lods[0])[i];
        minH = std::min(minH, h);
        maxH = std::max(maxH, h);
    }

    const uint32_t n2 = meta.GetLodVertexCount(2);
    for (uint32_t i = 0; i < n2; ++i) {
        const float h = AsFloats(tile.lods[2])[i];
        EXPECT_GE(h, minH - 1e-3f);
        EXPECT_LE(h, maxH + 1e-3f);
    }
}

TEST(TerrainGeneratorTest, GenerateTaskMatchesDirect)
{
    const auto meta = MakeMeta();
    const auto config = MakeConfig();

    auto task = CounterPtr<TerrainTileGenerateTask>(new TerrainTileGenerateTask());
    task->Setup(&config, &meta, {2, -1});
    task->StartAsync();
    TaskExecutor::Get()->WaitForAll();

    ASSERT_TRUE(task->IsFinished());

    const auto direct = GenerateTerrainTile(config, meta, {2, -1});
    EXPECT_EQ(task->GetPayload().lods[0].height, direct.lods[0].height);
    EXPECT_EQ(task->GetPayload().coord, direct.coord);
}

TEST(TerrainGeneratorTest, OnDemandStreamingGeneratesTiles)
{
    TerrainMeta meta;
    meta.tileSize     = 8;
    meta.resolution   = 1.f;
    meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
    meta.heightScale  = 1.f;
    meta.heightOffset = 0.f;
    meta.lodCount     = 1;

    TerrainAssetData data;
    data.meta = meta;   // no cooked tiles

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(6.f, 9.f);
    system.SetLoadBudget(50);
    system.SetGenerationEnabled(true);
    system.SetGenerateConfig(MakeConfig());

    Settle(system);

    EXPECT_GT(system.GetLoadedTileCount(), 0u);

    float height = 0.f;
    EXPECT_TRUE(system.GetField().QueryHeight(Vector3(4.f, 0.f, 4.f), height));
}
