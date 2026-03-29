//
// Created by blues on 2024/11/12.
//

#include <gtest/gtest.h>
#include <terrain/TerrainClipmap.h>

using namespace sky;

TEST(TerrainTest, ClipmapBasicTest)
{
    ClipmapConfig config = {};
    config.blockSize = 64;
    config.numLevels = 4;
    config.resolution = 1.f;

    TerrainClipmap clipmap;
    clipmap.Init(config);

    clipmap.UpdateSnapPositions(Vector3(100.f, 0.f, 100.f));

    ASSERT_EQ(clipmap.GetNumLevels(), 4u);
    ASSERT_FALSE(clipmap.GetVisibleBlocks().empty());
}