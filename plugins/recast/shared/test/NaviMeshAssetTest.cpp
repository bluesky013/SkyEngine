//
// Created on 2026/09/21.
//

#include <navigation/NaviMeshAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <core/file/FileSystem.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

using namespace sky;
using namespace sky::ai;

namespace {

    FileSystemPtr CreateTestFileSystem()
    {
        const auto dir = (std::filesystem::temp_directory_path() / "sky_navmesh_asset_test").string();
        return new NativeFileSystem(FilePath(dir));
    }

    std::vector<char> Encode(const NaviMeshData &data)
    {
        OMemoryArchive      stream;
        BinaryOutputArchive archive(stream);
        data.Save(archive);
        EXPECT_TRUE(archive.IsValid());
        return {stream.Data(), stream.Data() + stream.Size()};
    }

    void PatchUInt32(std::vector<char> &bytes, size_t offset, uint32_t value)
    {
        ASSERT_LE(offset + sizeof(value), bytes.size());
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    void ExpectRejected(const std::vector<char> &bytes)
    {
        IMemoryArchive     stream(bytes.data(), bytes.size());
        BinaryInputArchive archive(stream);
        NaviMeshData       data;
        data.tiles.push_back({1, 2, 3, {4, 5}});
        data.fullData = {6, 7};
        data.Load(archive);
        EXPECT_FALSE(archive.IsValid());
        EXPECT_TRUE(data.tiles.empty());
        EXPECT_TRUE(data.fullData.empty());
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
        EXPECT_TRUE(archive.IsValid());
    }

    NaviMeshData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
        EXPECT_TRUE(archive.IsValid());
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
        EXPECT_TRUE(archive.IsValid());
    }

    NaviMeshData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
        EXPECT_TRUE(archive.IsValid());
    }

    EXPECT_EQ(dst.mode, NaviMeshExportMode::Full);
    EXPECT_TRUE(dst.tiles.empty());
    EXPECT_EQ(dst.fullData, src.fullData);
}

TEST(NaviMeshAssetTest, RejectsDeclaredTileCountBeforeAllocation)
{
    auto         bytes           = Encode({});
    const size_t tileCountOffset = bytes.size() - sizeof(uint32_t) * 2;
    PatchUInt32(bytes, tileCountOffset, std::numeric_limits<uint32_t>::max());
    ExpectRejected(bytes);

    PatchUInt32(bytes, tileCountOffset, 1);
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, RejectsTilePayloadSizeBeforeAllocation)
{
    NaviMeshData data;
    data.tiles.push_back({1, 2, 3, {4, 5, 6}});
    auto         bytes             = Encode(data);
    const size_t payloadSizeOffset = bytes.size() - sizeof(uint32_t) * 2 - data.tiles.front().data.size();
    PatchUInt32(bytes, payloadSizeOffset, std::numeric_limits<uint32_t>::max());
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, RejectsFullPayloadSizeBeforeAllocation)
{
    NaviMeshData data;
    data.mode                      = NaviMeshExportMode::Full;
    data.fullData                  = {1, 2, 3};
    auto         bytes             = Encode(data);
    const size_t payloadSizeOffset = bytes.size() - sizeof(uint32_t) - data.fullData.size();
    PatchUInt32(bytes, payloadSizeOffset, std::numeric_limits<uint32_t>::max());
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, RejectsDuplicateTileLayers)
{
    NaviMeshData data;
    data.tiles.push_back({1, 2, 3, {4, 5, 6}});
    data.tiles.push_back({1, 2, 4, {7, 8}});
    auto         bytes            = Encode(data);
    const size_t firstTileOffset  = Encode({}).size() - sizeof(uint32_t);
    const size_t secondTileOffset = firstTileOffset + sizeof(uint32_t) * 4 + data.tiles.front().data.size();
    PatchUInt32(bytes, secondTileOffset + sizeof(uint32_t) * 2, data.tiles.front().layer);
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, RejectsTruncationWithoutLeavingPartialData)
{
    NaviMeshData data;
    data.tiles.push_back({1, 2, 3, {4, 5, 6}});
    auto bytes = Encode(data);
    bytes.pop_back();
    ExpectRejected(bytes);

    bytes.resize(sizeof(uint32_t));
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, RejectsUnknownModeAndVersion)
{
    auto bytes = Encode({});
    PatchUInt32(bytes, 0, 2);
    ExpectRejected(bytes);

    bytes = Encode({});
    PatchUInt32(bytes, sizeof(uint32_t), 2);
    ExpectRejected(bytes);
}

TEST(NaviMeshAssetTest, SaveRejectsInvalidModeAndDuplicateTileLayers)
{
    NaviMeshData        data;
    OMemoryArchive      stream;
    BinaryOutputArchive archive(stream);
    data.mode = static_cast<NaviMeshExportMode>(2);
    data.Save(archive);
    EXPECT_FALSE(archive.IsValid());
    EXPECT_EQ(stream.Size(), 0u);

    data.mode = NaviMeshExportMode::Tiled;
    data.tiles.push_back({1, 2, 3, {4}});
    data.tiles.push_back({1, 2, 3, {5}});
    OMemoryArchive      duplicateStream;
    BinaryOutputArchive duplicateArchive(duplicateStream);
    data.Save(duplicateArchive);
    EXPECT_FALSE(duplicateArchive.IsValid());
    EXPECT_EQ(duplicateStream.Size(), 0u);
}
