//
// Texture resource tests: compile + metadata of the texture types, the atlas
// packer, and device-backed create/upload smoke.
//

#include <aurora/resource/Texture.h>
#include <aurora/resource/TextureAtlas.h>

#include "AuroraTestHelper.h"

#include <gtest/gtest.h>

using namespace sky::aurora;
using namespace sky::aurora::test;

TEST(TextureResourceTest, DimensionDefaults)
{
    Texture2D tex2d;
    tex2d.Init(nullptr, PixelFormat::RGBA8_UNORM, {64, 32});
    EXPECT_EQ(tex2d.GetDescriptor().imageType, ImageType::IMAGE_2D);
    EXPECT_EQ(tex2d.GetArrayLayers(), 1u);
    EXPECT_EQ(tex2d.GetExtent().width, 64u);
    EXPECT_EQ(tex2d.GetExtent().height, 32u);
    EXPECT_EQ(tex2d.GetFormat(), PixelFormat::RGBA8_UNORM);

    TextureCube cube;
    cube.Init(nullptr, PixelFormat::RGBA8_UNORM, {64, 64});
    EXPECT_EQ(cube.GetArrayLayers(), 6u);
    EXPECT_TRUE(cube.GetDescriptor().viewUsage.TestBit(ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE));

    Texture2DArray array;
    array.Init(nullptr, PixelFormat::RGBA8_UNORM, {64, 64}, 8);
    EXPECT_EQ(array.GetArrayLayers(), 8u);

    Texture3D tex3d;
    tex3d.Init(nullptr, PixelFormat::RGBA8_UNORM, {16, 16, 4});
    EXPECT_EQ(tex3d.GetDescriptor().imageType, ImageType::IMAGE_3D);
    EXPECT_EQ(tex3d.GetExtent().depth, 4u);
    EXPECT_EQ(tex3d.GetArrayLayers(), 1u);
}

TEST(TextureResourceTest, TextureLinearAllocatorPack)
{
    TextureLinearAllocator allocator(64, 64);

    auto p1 = allocator.Allocate(32, 32);
    ASSERT_TRUE(p1.first);
    EXPECT_EQ(p1.second.x, 0u);
    EXPECT_EQ(p1.second.y, 0u);

    auto p2 = allocator.Allocate(32, 32);
    ASSERT_TRUE(p2.first);
    EXPECT_EQ(p2.second.x, 32u);
    EXPECT_EQ(p2.second.y, 0u);

    auto p3 = allocator.Allocate(32, 32);
    ASSERT_TRUE(p3.first);
    EXPECT_EQ(p3.second.x, 0u);
    EXPECT_EQ(p3.second.y, 32u);

    auto p4 = allocator.Allocate(32, 32);
    ASSERT_TRUE(p4.first);

    auto p5 = allocator.Allocate(32, 32);
    EXPECT_FALSE(p5.first);
}

TEST_F(AuroraVulkanTest, TextureLazyCreateAndUpload)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Texture2D tex;
    ASSERT_TRUE(tex.Init(device, PixelFormat::RGBA8_UNORM, {16, 16}));
    EXPECT_FALSE(tex.IsCreated());
    EXPECT_EQ(tex.GetImage(), nullptr);

    std::vector<uint8_t> data(16 * 16 * 4, 0x42);
    ASSERT_TRUE(tex.Upload(data.data(), data.size()));

    EXPECT_TRUE(tex.IsCreated());
    EXPECT_NE(tex.GetImage(), nullptr);
    EXPECT_EQ(tex.GetMipLevels(), 1u);
    EXPECT_EQ(tex.GetFormat(), PixelFormat::RGBA8_UNORM);
}

TEST_F(AuroraVulkanTest, TextureAtlasAllocateAndUpload)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    TextureAtlas atlas;
    ASSERT_TRUE(atlas.Init(device, PixelFormat::RGBA8_UNORM, {64, 64}));

    auto page = atlas.Allocate(16, 16);
    ASSERT_TRUE(page.first);
    EXPECT_EQ(page.second.x, 0u);
    EXPECT_EQ(page.second.y, 0u);

    std::vector<uint8_t> data(16 * 16 * 4, 0x11);
    ASSERT_TRUE(atlas.Upload(page.second, data.data(), data.size()));
    EXPECT_TRUE(atlas.IsCreated());

    atlas.SetAllocator(std::make_unique<TextureLinearAllocator>(64, 64));
    auto page2 = atlas.Allocate(16, 16);
    ASSERT_TRUE(page2.first);
    EXPECT_EQ(page2.second.x, 0u);
    EXPECT_EQ(page2.second.y, 0u);
}
