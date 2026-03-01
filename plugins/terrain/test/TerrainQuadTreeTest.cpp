//
// Created by blues on 2024/11/12.
//

#include <gtest/gtest.h>
#include <terrain/TerrainQuadTree.h>

using namespace sky;


TEST(TerrainTest, QuadTreeTest)
{
    TerrainConfig config = {};
    config.maxExt = 16;
    config.leafExt = 4;
    config.resolution = 1.f;

    TerrainQuadTree quadTree(config, VEC3_ZERO);
    quadTree.Split(Vector3(6.f, 0.f, 6.f));

    ASSERT_EQ(1, 1);
}