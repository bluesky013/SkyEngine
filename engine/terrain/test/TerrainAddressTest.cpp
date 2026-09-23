//
// Created on 2026/09/22.
//

#include <terrain/TerrainAddress.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

TEST(TerrainAddressTest, WorldToTileAndBack)
{
    TerrainMeta meta;
    meta.tileSize   = 64;
    meta.resolution = 1.f;
    meta.origin     = VEC3_ZERO;

    const float tileWorld = meta.GetTileWorldSize();
    ASSERT_FLOAT_EQ(tileWorld, 64.f);

    Vector3 point(tileWorld * 1.5f, 0.f, tileWorld * 2.25f);
    auto coord = WorldToTile(meta, point);
    EXPECT_EQ(coord.x, 1);
    EXPECT_EQ(coord.y, 2);

    Vector3 origin = TileToWorld(meta, coord);
    EXPECT_FLOAT_EQ(origin.x, tileWorld);
    EXPECT_FLOAT_EQ(origin.z, tileWorld * 2.f);

    for (float dx = 1.f; dx < tileWorld; dx += 7.f) {
        for (float dz = 1.f; dz < tileWorld; dz += 7.f) {
            Vector3 inside(origin.x + dx, 0.f, origin.z + dz);
            EXPECT_EQ(WorldToTile(meta, inside), coord);
        }
    }
}

TEST(TerrainAddressTest, OriginOffset)
{
    TerrainMeta meta;
    meta.tileSize   = 32;
    meta.resolution = 0.5f;
    meta.origin     = Vector3(-100.f, 5.f, 200.f);

    const float tileWorld = meta.GetTileWorldSize();
    ASSERT_FLOAT_EQ(tileWorld, 16.f);

    Vector3 point(meta.origin.x + tileWorld * 3.25f, 0.f, meta.origin.z - tileWorld * 0.5f);
    auto coord = WorldToTile(meta, point);
    EXPECT_EQ(coord.x, 3);
    EXPECT_EQ(coord.y, -1);

    Vector3 origin = TileToWorld(meta, coord);
    EXPECT_FLOAT_EQ(origin.x, meta.origin.x + tileWorld * 3.f);
    EXPECT_FLOAT_EQ(origin.z, meta.origin.z - tileWorld * 1.f);
}

TEST(TerrainAddressTest, LocalTexelWrapsWithinTile)
{
    TerrainMeta meta;
    meta.tileSize   = 64;
    meta.resolution = 1.f;

    const float tileWorld = meta.GetTileWorldSize();

    float localX = 0.f;
    float localZ = 0.f;

    WorldToLocalTexel(meta, Vector3(tileWorld * 2.f + 5.f, 0.f, tileWorld * 3.f + 10.f), localX, localZ);
    EXPECT_FLOAT_EQ(localX, 5.f);
    EXPECT_FLOAT_EQ(localZ, 10.f);

    WorldToLocalTexel(meta, Vector3(-3.f, 0.f, -1.f), localX, localZ);
    EXPECT_FLOAT_EQ(localX, tileWorld - 3.f);
    EXPECT_FLOAT_EQ(localZ, tileWorld - 1.f);
}
