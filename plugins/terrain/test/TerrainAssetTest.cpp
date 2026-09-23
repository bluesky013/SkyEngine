//
// Created on 2026/09/22.
//

#include <terrain/TerrainAsset.h>

#include <core/file/FileSystem.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace sky;
using namespace sky::terrain;

namespace {

    FileSystemPtr CreateTestFileSystem()
    {
        const auto dir = (std::filesystem::temp_directory_path() / "sky_terrain_asset_test").string();
        return new NativeFileSystem(FilePath(dir));
    }

    TerrainAssetData BuildSampleData()
    {
        TerrainAssetData data;
        data.version              = 1;
        data.meta.tileSize        = 4;
        data.meta.resolution      = 1.f;
        data.meta.heightFormat    = TerrainHeightFormat::R32_SFLOAT;
        data.meta.heightScale     = 1.f;
        data.meta.heightOffset    = 0.f;
        data.meta.tileCountX      = 2;
        data.meta.tileCountY      = 1;
        data.meta.layerCount      = 5;
        data.meta.lodCount        = 3;
        data.meta.origin          = Vector3(10.f, 0.f, -20.f);

        TerrainTileInfo info;
        info.coord     = TerrainTileCoord{0, 0};
        info.bounds    = AABB(Vector3(10.f, -1.f, -20.f), Vector3(14.f, 3.f, -16.f));
        info.minHeight = -1.f;
        info.maxHeight = 3.f;
        info.lodCount  = 3;
        data.manifest.push_back(info);

        TerrainTilePayload tile;
        tile.coord = TerrainTileCoord{0, 0};
        for (uint32_t lod = 0; lod < data.meta.lodCount; ++lod) {
            TerrainLodPayload lodPayload;
            lodPayload.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(lod)) * sizeof(float), 0);
            if (lod == 0) {
                lodPayload.splat.push_back({1, 2, 3, 4, 5, 6, 7, 8});
                lodPayload.splat.push_back({9, 10});
            }
            tile.lods.push_back(std::move(lodPayload));
        }
        data.tiles.push_back(std::move(tile));

        return data;
    }

} // namespace

TEST(TerrainAssetTest, SaveLoadRoundTrip)
{
    const auto src = BuildSampleData();

    auto fs   = CreateTestFileSystem();
    auto file = fs->CreateOrOpenFile(FilePath("terrain.bin"));
    ASSERT_NE(file, nullptr);

    {
        auto                oa = file->WriteAsArchive();
        BinaryOutputArchive archive(*oa);
        src.Save(archive);
    }

    TerrainAssetData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
    }

    EXPECT_EQ(dst.version, 1u);
    EXPECT_EQ(dst.meta.tileSize, 4u);
    EXPECT_EQ(dst.meta.heightFormat, TerrainHeightFormat::R32_SFLOAT);
    EXPECT_EQ(dst.meta.layerCount, 5u);
    EXPECT_EQ(dst.meta.lodCount, 3u);
    EXPECT_EQ(dst.meta.GetSplatTileCount(), 2u);
    EXPECT_FLOAT_EQ(dst.meta.origin.x, 10.f);
    EXPECT_FLOAT_EQ(dst.meta.origin.z, -20.f);

    ASSERT_EQ(dst.manifest.size(), 1u);
    EXPECT_EQ(dst.manifest[0].coord.x, 0);
    EXPECT_EQ(dst.manifest[0].lodCount, 3u);
    EXPECT_FLOAT_EQ(dst.manifest[0].minHeight, -1.f);
    EXPECT_FLOAT_EQ(dst.manifest[0].bounds.max.y, 3.f);

    ASSERT_EQ(dst.tiles.size(), 1u);
    EXPECT_EQ(dst.tiles[0].coord, src.tiles[0].coord);
    ASSERT_EQ(dst.tiles[0].lods.size(), 3u);
    for (uint32_t lod = 0; lod < dst.tiles[0].lods.size(); ++lod) {
        EXPECT_EQ(dst.tiles[0].lods[lod].height, src.tiles[0].lods[lod].height);
    }
    ASSERT_EQ(dst.tiles[0].lods[0].splat.size(), 2u);
    EXPECT_EQ(dst.tiles[0].lods[0].splat[0], src.tiles[0].lods[0].splat[0]);
    EXPECT_EQ(dst.tiles[0].lods[0].splat[1], src.tiles[0].lods[0].splat[1]);
    EXPECT_TRUE(dst.tiles[0].lods[1].splat.empty());
}

TEST(TerrainAssetTest, AddressTileLod)
{
    const auto data = BuildSampleData();

    ASSERT_EQ(data.tiles.size(), 1u);
    const auto &tile = data.tiles[0];
    EXPECT_EQ(tile.GetLodCount(), 3u);
    EXPECT_NE(tile.GetLod(0), nullptr);
    EXPECT_NE(tile.GetLod(2), nullptr);
    EXPECT_EQ(tile.GetLod(3), nullptr);
    EXPECT_LT(tile.GetLod(2)->height.size(), tile.GetLod(0)->height.size());
}

TEST(TerrainAssetTest, SplatTileCountRoundsUp)
{
    TerrainMeta meta;
    meta.layerCount = 4;
    EXPECT_EQ(meta.GetSplatTileCount(), 1u);

    meta.layerCount = 1;
    EXPECT_EQ(meta.GetSplatTileCount(), 1u);

    meta.layerCount = 9;
    EXPECT_EQ(meta.GetSplatTileCount(), 3u);
}
