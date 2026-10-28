//
// Aurora asset header + bulk round-trip tests: the header loads without the bulk,
// and ReadBulk reads the payload on demand (image slices; mesh vertex/index/skin).
//

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/rhi/VertexSemantic.h>
#include <aurora/adaptor/assets/SkinAsset.h>

#include <core/math/Matrix4.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace sky;

namespace {

    NativeFileSystemPtr MakeBundle(const std::string &key, const char *dirName)
    {
        const auto bundleRoot = std::filesystem::temp_directory_path() / dirName;
        std::filesystem::remove_all(bundleRoot);
        NativeFileSystemPtr fs = new NativeFileSystem(FilePath((bundleRoot / "b").string()));
        AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(fs, key));
        return fs;
    }

} // namespace

class AuroraAssetBulkTest : public ::testing::Test {};

TEST_F(AuroraAssetBulkTest, ImageBulkRoundTrip)
{
    MakeBundle("imgbulk", "sky_aurora_image_bulk");

    const Uuid                 id     = Uuid::Create();
    const std::vector<uint8_t> pixels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    {
        auto asset = std::static_pointer_cast<Asset<aurora::Texture>>(
            AssetManager::Get()->CreateBuildAsset(id, Name(AssetTraits<aurora::Texture>::ASSET_TYPE.data())));
        ASSERT_NE(asset, nullptr);
        auto &data       = asset->Data();
        data.format      = aurora::PixelFormat::RGBA8_UNORM;
        data.type        = aurora::ImageAssetType::TEXTURE_2D;
        data.width       = 2;
        data.height      = 2;
        data.mipLevels   = 1;
        data.arrayLayers = 1;
        data.dataSize    = pixels.size();
        aurora::ImageSliceHeader slice;
        slice.offset = 0;
        slice.size   = pixels.size();
        data.slices.push_back(slice);
        asset->AddBulk(pixels.data(), pixels.size());
        ASSERT_TRUE(AssetManager::Get()->SaveAsset(asset, "imgbulk"));
    }

    auto loaded = AssetManager::Get()->LoadAsset<aurora::Texture>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());

    EXPECT_EQ(loaded->Data().width, 2u);
    EXPECT_EQ(loaded->Data().dataSize, pixels.size());
    ASSERT_EQ(loaded->Data().slices.size(), 1u);
    EXPECT_TRUE(loaded->HasBulk());
    EXPECT_EQ(loaded->ReadBulk(loaded->Data().slices[0].offset, loaded->Data().slices[0].size), pixels);

    loaded.reset();
}

TEST_F(AuroraAssetBulkTest, MeshBulkAndIndependentSkinRoundTrip)
{
    MakeBundle("meshbulk", "sky_aurora_mesh_bulk");

    const Uuid                 id  = Uuid::Create();
    const std::vector<uint8_t> vtx = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const std::vector<uint8_t> idx = {0, 0, 0, 0, 0, 0};

    const Uuid skinId = Uuid::Create();

    {
        auto asset = std::static_pointer_cast<Asset<aurora::Mesh>>(
            AssetManager::Get()->CreateBuildAsset(id, Name(AssetTraits<aurora::Mesh>::ASSET_TYPE.data())));
        ASSERT_NE(asset, nullptr);
        auto &data        = asset->Data();
        data.vertexStride = vtx.size();
        data.vertexCount  = 1;
        data.vertexOffset = 0;
        data.vertexSize   = vtx.size();
        data.indexOffset  = vtx.size();
        data.indexSize    = idx.size();
        data.indexCount   = 3;
        data.indexType    = static_cast<uint32_t>(aurora::IndexType::U16);
        data.attributes.push_back({static_cast<uint8_t>(aurora::VertexSemantic::POSITION), 0, 0, static_cast<uint32_t>(aurora::Format::F_RGB32), 0});
        data.subMeshes.push_back({0, 3, 0});

        asset->AddBulk(vtx.data(), vtx.size());
        asset->AddBulk(idx.data(), idx.size());
        auto skinAsset = std::static_pointer_cast<Asset<aurora::Skin>>(
            AssetManager::Get()->CreateBuildAsset(skinId, Name(AssetTraits<aurora::Skin>::ASSET_TYPE.data())));
        ASSERT_NE(skinAsset, nullptr);
        auto &skinData         = skinAsset->Data();
        skinData.sourceOwner   = id;
        skinData.binding.input = {42, {"slot0", "slot1"}};
        skinData.binding.binds = {{Matrix4::Identity(), "root", 0}, {Matrix4::Identity(), "spine", 0}};
        const std::array<AssetPtr, 2> products{asset, skinAsset};
        ASSERT_TRUE(AssetManager::Get()->SaveProducts(products, "meshbulk"));
    }

    auto loaded = AssetManager::Get()->LoadAsset<aurora::Mesh>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());

    EXPECT_EQ(loaded->ReadBulk(loaded->Data().vertexOffset, loaded->Data().vertexSize), vtx);
    EXPECT_EQ(loaded->ReadBulk(loaded->Data().indexOffset, loaded->Data().indexSize), idx);

    auto skinAsset = AssetManager::Get()->LoadAsset<aurora::Skin>(skinId);
    ASSERT_NE(skinAsset, nullptr);
    skinAsset->BlockUntilLoaded();
    ASSERT_TRUE(skinAsset->IsLoaded());
    const auto skin = aurora::FetchOrCreateSkinByAsset(skinAsset);
    ASSERT_NE(skin, nullptr);
    EXPECT_EQ(skin->GetData().binds.size(), 2U);
    EXPECT_EQ(skin->GetData().binds[1].targetBone, "spine");
    EXPECT_EQ(skinAsset->Data().sourceOwner, id);

    loaded.reset();
}
