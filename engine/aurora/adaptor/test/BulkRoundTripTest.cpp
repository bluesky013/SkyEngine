//
// Aurora asset header + bulk round-trip tests: the header loads without the bulk,
// and ReadBulk reads the payload on demand (image slices; mesh vertex/index/skin).
//

#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>

#include <core/math/Matrix4.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

#include <gtest/gtest.h>

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

class AuroraAssetBulkTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        aurora::AuroraReflection(SerializationContext::Get());
    }
};

TEST_F(AuroraAssetBulkTest, ImageBulkRoundTrip)
{
    MakeBundle("imgbulk", "sky_aurora_image_bulk");

    const Uuid                 id     = Uuid::Create();
    const std::vector<uint8_t> pixels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<aurora::Texture>(id);
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
        AssetManager::Get()->SaveAsset(asset, "imgbulk");
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

TEST_F(AuroraAssetBulkTest, MeshBulkAndSkinRoundTrip)
{
    MakeBundle("meshbulk", "sky_aurora_mesh_bulk");

    const Uuid                 id  = Uuid::Create();
    const std::vector<uint8_t> vtx = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const std::vector<uint8_t> idx = {0, 1, 2};

    const std::vector<Matrix4>  bindMatrices = {Matrix4::Identity(), Matrix4::Identity()};
    const std::vector<uint32_t> boneMapping  = {0, 1};

    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<aurora::Mesh>(id);
        ASSERT_NE(asset, nullptr);
        auto &data            = asset->Data();
        data.vertexStride     = vtx.size();
        data.vertexCount      = 1;
        data.vertexOffset     = 0;
        data.vertexSize       = vtx.size();
        data.indexOffset      = vtx.size();
        data.indexSize        = idx.size();
        data.indexCount       = 3;
        data.indexType        = 0;
        data.skinBoneNames    = {"root", "spine"};
        data.skinBoneCount    = static_cast<uint32_t>(bindMatrices.size());
        data.skinMappingCount = static_cast<uint32_t>(boneMapping.size());
        data.skinOffset       = data.indexOffset + data.indexSize;
        data.skinData.resize(data.skinBoneCount * sizeof(Matrix4) + data.skinMappingCount * sizeof(uint32_t));
        std::memcpy(data.skinData.data(), bindMatrices.data(), bindMatrices.size() * sizeof(Matrix4));
        std::memcpy(data.skinData.data() + data.skinBoneCount * sizeof(Matrix4), boneMapping.data(), boneMapping.size() * sizeof(uint32_t));
        data.skinSize = data.skinData.size();

        asset->AddBulk(vtx.data(), vtx.size());
        asset->AddBulk(idx.data(), idx.size());
        asset->AddBulk(data.skinData.data(), data.skinData.size());
        AssetManager::Get()->SaveAsset(asset, "meshbulk");
    }

    auto loaded = AssetManager::Get()->LoadAsset<aurora::Mesh>(id);
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());

    EXPECT_EQ(loaded->ReadBulk(loaded->Data().vertexOffset, loaded->Data().vertexSize), vtx);
    EXPECT_EQ(loaded->ReadBulk(loaded->Data().indexOffset, loaded->Data().indexSize), idx);

    auto skin = aurora::CreateSkinFromMeshAsset(*loaded);
    ASSERT_NE(skin, nullptr);
    EXPECT_EQ(skin->GetInverseBindMatrices().size(), 2u);
    ASSERT_EQ(skin->GetBoneMapping().size(), 2u);
    EXPECT_EQ(skin->GetBoneMapping()[1], 1u);

    loaded.reset();
}
