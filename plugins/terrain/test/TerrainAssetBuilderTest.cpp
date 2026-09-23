//
// Created on 2026/09/22.
//

#include <terrain/TerrainAssetBuilder.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

namespace {

    TerrainSourceData MakeSource()
    {
        TerrainSourceData source;
        source.tileSize     = 8;
        source.resolution   = 1.f;
        source.heightFormat = static_cast<uint32_t>(TerrainHeightFormat::R32_SFLOAT);
        source.decodeScale  = 1.f;
        source.decodeOffset = 0.f;
        source.tileCountX   = 2;
        source.tileCountY   = 1;
        source.tileStartX   = 0;
        source.tileStartY   = 0;
        source.originX      = 0.f;
        source.originY      = 0.f;
        source.originZ      = 0.f;
        source.seed         = 42;
        source.baseFrequency = 0.05f;
        source.octaves      = 4;
        source.heightScale  = 10.f;
        source.lodCount     = 3;
        source.layerCount   = 0;
        return source;
    }

} // namespace

TEST(TerrainAssetBuilderTest, BuildsTilesAndManifest)
{
    const auto source = MakeSource();

    TerrainAssetData data;
    ASSERT_TRUE(BuildTerrainAsset(source, data));

    EXPECT_EQ(data.meta.tileSize, 8u);
    EXPECT_EQ(data.meta.lodCount, 3u);
    EXPECT_EQ(data.tiles.size(), 2u);
    EXPECT_EQ(data.manifest.size(), 2u);

    for (const auto &tile : data.tiles) {
        EXPECT_EQ(tile.GetLodCount(), 3u);
    }

    for (const auto &info : data.manifest) {
        EXPECT_LE(info.minHeight, info.maxHeight);
        EXPECT_FLOAT_EQ(info.bounds.max.x - info.bounds.min.x, 8.f);
        EXPECT_FLOAT_EQ(info.bounds.max.z - info.bounds.min.z, 8.f);
        EXPECT_EQ(info.lodCount, 3u);
    }

    // Tile (0,0) spans [0,8], tile (1,0) spans [8,16].
    EXPECT_EQ(data.manifest[0].coord, (TerrainTileCoord{0, 0}));
    EXPECT_EQ(data.manifest[1].coord, (TerrainTileCoord{1, 0}));
}

TEST(TerrainAssetBuilderTest, Deterministic)
{
    const auto source = MakeSource();

    TerrainAssetData a;
    TerrainAssetData b;
    ASSERT_TRUE(BuildTerrainAsset(source, a));
    ASSERT_TRUE(BuildTerrainAsset(source, b));

    ASSERT_EQ(a.tiles.size(), b.tiles.size());
    for (size_t i = 0; i < a.tiles.size(); ++i) {
        EXPECT_EQ(a.tiles[i].lods[0].height, b.tiles[i].lods[0].height);
        EXPECT_EQ(a.tiles[i].lods[0].splat, b.tiles[i].lods[0].splat);
    }
}

TEST(TerrainAssetBuilderTest, RejectsInvalidLayout)
{
    auto source = MakeSource();
    source.tileSize = 0;

    TerrainAssetData data;
    EXPECT_FALSE(BuildTerrainAsset(source, data));
}
