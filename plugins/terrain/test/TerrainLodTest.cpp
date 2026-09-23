//
// Created on 2026/09/22.
//

#include <terrain/TerrainLod.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

TEST(TerrainLodTest, BuildLevelDescAndLodMapping)
{
    TerrainMeta meta;
    meta.tileSize   = 64;
    meta.resolution = 2.f;
    meta.lodCount   = 3;

    auto desc = BuildTerrainLodDesc(meta, 5);
    ASSERT_EQ(desc.GetNumLevels(), 5u);
    EXPECT_EQ(desc.blockSize, 64u);
    EXPECT_FLOAT_EQ(desc.resolution, 2.f);

    for (uint32_t level = 0; level < desc.GetNumLevels(); ++level) {
        EXPECT_EQ(desc.levels[level].level, level);
        EXPECT_FLOAT_EQ(desc.levels[level].scale, 2.f * static_cast<float>(1u << level));
        EXPECT_FLOAT_EQ(desc.levels[level].blockWorldSize, 64.f * desc.levels[level].scale);
    }

    EXPECT_EQ(desc.levels[0].lod, 0u);
    EXPECT_EQ(desc.levels[1].lod, 1u);
    EXPECT_EQ(desc.levels[2].lod, 2u);
    EXPECT_EQ(desc.levels[3].lod, 2u);
    EXPECT_EQ(desc.levels[4].lod, 2u);
}

TEST(TerrainLodTest, LodGeometryHalvesDownToMinimum)
{
    TerrainMeta meta;
    meta.tileSize = 8;
    meta.lodCount = 4;

    EXPECT_EQ(meta.GetLodQuadCount(0), 8u);
    EXPECT_EQ(meta.GetLodQuadCount(1), 4u);
    EXPECT_EQ(meta.GetLodQuadCount(2), 2u);
    EXPECT_EQ(meta.GetLodQuadCount(3), 1u);
    EXPECT_EQ(meta.GetLodQuadCount(4), 1u);

    EXPECT_EQ(meta.GetLodVertexSize(0), 9u);
    EXPECT_EQ(meta.GetLodVertexCount(1), 25u);
    EXPECT_EQ(meta.GetClampedLod(9), 3u);
}
