//
// Created on 2026/09/21.
//

#include <navigation/NaviMeshAsset.h>

#include <core/file/FileSystem.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace sky;
using namespace sky::ai;

namespace {

    FileSystemPtr CreateTestFileSystem()
    {
        const auto dir = (std::filesystem::temp_directory_path() / "sky_navmesh_asset_test").string();
        return new NativeFileSystem(FilePath(dir));
    }

} // namespace

TEST(NaviMeshAssetTest, SaveLoadRoundTripTiled)
{
    NaviMeshData src;
    src.mode              = NaviMeshExportMode::Tiled;
    src.params.version    = 1;
    src.params.agent      = NaviAgentConfig{1.8f, 0.4f, 45.f, 0.3f};
    src.params.resolution = NaviMeshResolution{0.3f, 0.2f, 10.f};
    src.params.bounds.min = Vector3(-50.f, -10.f, -50.f);
    src.params.bounds.max = Vector3(50.f, 10.f, 50.f);
    src.tiles.push_back(NaviMeshTilePayload{1, 2, 0, {1, 2, 3, 4}});
    src.tiles.push_back(NaviMeshTilePayload{3, 4, 1, {5, 6}});

    auto fs   = CreateTestFileSystem();
    auto file = fs->CreateOrOpenFile(FilePath("nav.bin"));
    ASSERT_NE(file, nullptr);

    {
        auto                oa = file->WriteAsArchive();
        BinaryOutputArchive archive(*oa);
        src.Save(archive);
    }

    NaviMeshData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
    }

    EXPECT_EQ(dst.mode, NaviMeshExportMode::Tiled);
    EXPECT_EQ(dst.params.version, 1u);
    EXPECT_FLOAT_EQ(dst.params.agent.radius, 0.4f);
    EXPECT_FLOAT_EQ(dst.params.resolution.cellSize, 0.3f);
    EXPECT_FLOAT_EQ(dst.params.bounds.max.x, 50.f);

    ASSERT_EQ(dst.tiles.size(), 2u);
    EXPECT_EQ(dst.tiles[0].tx, 1);
    EXPECT_EQ(dst.tiles[0].ty, 2);
    EXPECT_EQ(dst.tiles[0].layer, 0u);
    EXPECT_EQ(dst.tiles[0].data, src.tiles[0].data);
    EXPECT_EQ(dst.tiles[1].layer, 1u);
    EXPECT_EQ(dst.tiles[1].data, src.tiles[1].data);
}

TEST(NaviMeshAssetTest, SaveLoadRoundTripFull)
{
    NaviMeshData src;
    src.mode     = NaviMeshExportMode::Full;
    src.fullData = {9, 8, 7, 6, 5};

    auto fs   = CreateTestFileSystem();
    auto file = fs->CreateOrOpenFile(FilePath("nav_full.bin"));
    ASSERT_NE(file, nullptr);

    {
        auto                oa = file->WriteAsArchive();
        BinaryOutputArchive archive(*oa);
        src.Save(archive);
    }

    NaviMeshData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
    }

    EXPECT_EQ(dst.mode, NaviMeshExportMode::Full);
    EXPECT_TRUE(dst.tiles.empty());
    EXPECT_EQ(dst.fullData, src.fullData);
}
